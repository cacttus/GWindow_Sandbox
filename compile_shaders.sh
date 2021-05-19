FILE=./shaderc/build/glslc/glslc
if test -f "$FILE"; then
  echo "Found glslc."
  ./shaderc/build/glslc/glslc -fshader-stage=vertex ./test.vs -o ./test_vs.spv
  ./shaderc/build/glslc/glslc -fshader-stage=fragment ./test.fs -o ./test_fs.spv
else 
  echo "./shaderc not found - Download shaderc and place in project root:"
  echo "https://github.com/google/shaderc"
fi

