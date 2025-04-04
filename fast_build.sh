#!/bin/bash
# Script to build the project without running Doxygen

# Create a modified Doxyfile that disables documentation generation
cp Doxyfile Doxyfile.bak
sed -i '' 's/GENERATE_HTML.*=.*YES/GENERATE_HTML = NO/g' Doxyfile
sed -i '' 's/GENERATE_LATEX.*=.*YES/GENERATE_LATEX = NO/g' Doxyfile
sed -i '' 's/GENERATE_XML.*=.*YES/GENERATE_XML = NO/g' Doxyfile

# Build the project
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel

# Restore the original Doxyfile
mv Doxyfile.bak Doxyfile 