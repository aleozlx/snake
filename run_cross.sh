#!/bin/bash
set -e
echo "@@@@ Sync..."
rsync -av build_x64/hello deck:bin/
echo "@@@@ Start..."
ssh deck bin/hello
