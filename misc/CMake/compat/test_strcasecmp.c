#include <strings.h>

int
main(void)
{
  const char s1[] = "one";
  const char s2[] = "two";

  int res __attribute__((unused)) = strcasecmp(s1, s2);

  return 0;
}
