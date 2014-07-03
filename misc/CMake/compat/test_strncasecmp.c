#include <strings.h>

int
main(void)
{
  const char s1[] = "one";
  const char s2[] = "two";
  const size_t n  = 2;

  int res __attribute__((unused)) = strncasecmp(s1, s2, n);

  return 0;
}
