#ifndef __DRIVER_H_
#define __DRIVER_H_

struct test_def {
    const char *name;
    int (*testfunc) (void);
};

void setup();

#endif /* __DRIVER_H_ */
