#!/bin/bash
FILE=$1
PASS=$(< $FILE)
LEN=${#PASS}

if [ $LEN -gt 32 ] || [ $LEN -lt 6 ]; then
  echo "Error: Password length invalid."
  exit
fi

PTS=$LEN

if /bin/egrep -q [#$\+%@] $FILE; then
  let PTS=PTS+5
fi

if /bin/egrep -q [0-9] $FILE; then
  let PTS=PTS+5
fi

if /bin/egrep -q [a-zA-Z] $FILE; then
  let PTS=PTS+5
fi

if /bin/egrep -q ".*[a-z][a-z][a-z].*" $FILE; then
  echo "rrr"
  let PTS=PTS-3
fi

if /bin/egrep -q .*[A-Z][A-Z][A-Z].* $FILE; then
  let PTS=PTS-3
fi

if /bin/egrep -q .*[0-9][0-9][0-9].* $FILE; then
  let PTS=PTS-3
fi

echo $PTS
