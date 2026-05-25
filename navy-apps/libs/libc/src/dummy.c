int isatty(int id)
{
  return (id == 0 || id == 1 || id == 2) ? 1 : 0;
}
