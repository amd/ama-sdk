# Simple Examples
This directory and its sub-folders contain simple to use XMA based applications for decode, encode and scaled transcoding. The goal in all these applications has been to introduce XMA and XRM programming flow. As such, typical error checking, argument compatibility, etc. have been ignored in favor of clarity and simplicity.

# Content

1. Root folder: Contains a simple Cmake file and build script.
1. __dec__ folder: Contains a simple app for decoding
1. __enc__ folder: Contains a simple app for encoding
1. __enc_cpp__ folder: Contains a simple app for encoding, in C++
1. __txc__ folder: Contains a simple app for transcoding
1. __lib__ folder: Contains simple initilaization and processing routines


None of the above apps attempt to parse the incoming stream and their respective configuration is defined statically through the local __amaconf.h__ file 
