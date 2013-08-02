#!/bin/sh

groff -mm -Tps burst.mm   > burst.ps

groff -mm -Thtml burst.mm > burst.html
