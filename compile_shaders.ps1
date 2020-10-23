If( Test-Path "..\shaderc\glslc\Debug\glslc.exe"){
  Write-Host "Found glslc."
  ..\shaderc\glslc\Debug\glslc.exe -fshader-stage=vertex ./test.vs -o ./test_vs.spv
  ..\shaderc\glslc\Debug\glslc.exe -fshader-stage=fragment ./test.fs -o ./test_fs.spv
}
Else{
  Write-Host "shaderc not found - Download/Build shaderc and place in ..\shaderc\glslc\Debug\"
  Write-Host "https://github.com/google/shaderc#downloads"
}
