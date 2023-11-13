#!/bin/bash

mkdir out

make BUILDDIR=./out HOSTFILE=etc/linux.c all
