#include "common.h"
#include "proc.h"

#define NAME(key) \
  [_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
    [_KEY_NONE] = "NONE",
    _KEYS(NAME)};

size_t events_read(void *buf, size_t len)
{
  if (len == 0)
  {
    return 0;
  }

  char *out = (char *)buf;

  static uint32_t last_time = 0;
  while (1)
  {
    int key = _read_key();
    if (key != _KEY_NONE)
    {
      bool is_down = (key & 0x8000) != 0;
      key &= 0x7fff;

      if (key == _KEY_F12 && is_down)
      {
        extern void toggle_game(void);
        toggle_game();
        continue;
      }

      const char *name = keyname[key];
      size_t pos = 0;
      const char *prefix = is_down ? "kd " : "ku ";
      size_t prefix_len = 3;

      while (pos < len && pos < prefix_len)
      {
        out[pos] = prefix[pos];
        pos++;
      }

      size_t name_len = strlen(name);
      size_t name_pos = 0;
      while (pos < len && name_pos < name_len)
      {
        out[pos++] = name[name_pos++];
      }

      if (pos < len)
      {
        out[pos++] = '\n';
      }

      return pos;
    }

    uint32_t now = _uptime();
    if (now - last_time >= 1000 / 30)
    {
      last_time = now;
      size_t pos = 0;
      if (pos < len)
      {
        out[pos++] = 't';
      }
      if (pos < len)
      {
        out[pos++] = ' ';
      }

      char tmp[16];
      size_t n = 0;
      uint32_t val = now;
      do
      {
        tmp[n++] = (char)('0' + (val % 10));
        val /= 10;
      } while (val != 0 && n < sizeof(tmp));

      while (n > 0 && pos < len)
      {
        out[pos++] = tmp[--n];
      }

      if (pos < len)
      {
        out[pos++] = '\n';
      }

      return pos;
    }
  }
}

static char dispinfo[128] __attribute__((used));

void dispinfo_read(void *buf, off_t offset, size_t len)
{
  size_t total = strlen(dispinfo);
  if ((size_t)offset >= total)
  {
    return;
  }
  if (offset + len > total)
  {
    len = total - offset;
  }
  memcpy(buf, dispinfo + offset, len);
}

void fb_write(const void *buf, off_t offset, size_t len)
{
  if (len == 0)
  {
    return;
  }
  uint32_t width = _screen.width;
  if (width == 0)
  {
    return;
  }
  size_t pixel_offset = offset / 4;
  size_t pixel_len = len / 4;
  const uint32_t *pixels = (const uint32_t *)buf;

  size_t x = pixel_offset % width;
  size_t y = pixel_offset / width;
  size_t remaining = pixel_len;

  while (remaining > 0)
  {
    size_t row_cap = width - x;
    size_t row_len = remaining < row_cap ? remaining : row_cap;
    _draw_rect(pixels, (int)x, (int)y, (int)row_len, 1);
    pixels += row_len;
    remaining -= row_len;
    x = 0;
    y += 1;
  }
}

void init_device()
{
  _ioe_init();

  // TODO: print the string to array `dispinfo` with the format
  // described in the Navy-apps convention
  snprintf(dispinfo, sizeof(dispinfo), "WIDTH:%d\nHEIGHT:%d\n",
           _screen.width, _screen.height);
}
