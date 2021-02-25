#!/bin/bash
rm -f svn_noncad.txt
SVN_REPO=/home/user/brlcad_repo/
svn log file://$SVN_REPO/osl | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/rt^3 | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/ova | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/isst | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/jbrlcad | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/geomcore | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/web | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/webcad | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/rtcmp | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
svn log file://$SVN_REPO/iBME | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq >> svn_noncad.txt
