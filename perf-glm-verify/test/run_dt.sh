#!/bin/bash

CURRENT_PATH=$(cd "$(dirname "$0")"; pwd)
cd "${CURRENT_PATH:?}"

main()
{
  hdt clean && hdt build && hdt run "--args=\"--gtest_output=xml:report.xml\""
  hdt report
  echo "done"
}

main
