#!/bin/bash

declare glslc_bin="./shaderc/build/glslc/glslc"
declare bin_path="./data/" #Output path
declare src_path="./src/shaders/"
declare debug=0 # 1 or 0

if [[ -f $glslc_bin ]]; then
  echo "Compiling shaders.."
  declare files=(${src_path}*)
  for i in ${files[@]}
  do
    declare src_file_full_path=$i
    declare basename=$(basename -- "$i")
    declare ext="${basename##*.}"
    basename="${basename%.*}"
    declare src_file=${basename}.${ext}
    declare output_ext=spv
    declare bin_file=${basename}.${ext}.${output_ext}
    
    if (( $debug )); then
      echo src_file=${src_file}
      echo basename=${basename}
      echo ext=${ext}
      echo output_ext=${output_ext}
      echo bin_file=${bin_file}
    fi
    if [[ -f ${bin_path}${bin_file} ]]; then
      declare bin_date=$(date -r ${bin_path}${bin_file})
      declare src_date=$(date -r ${src_path}${src_file})
      if (( $debug )); then
        echo bin_date=${bin_date}
        echo src_date=${src_date}
      fi

      if (( $(date -d "${bin_date}" +%s) > $(date -d "${src_date}" +%s) )); then
        if(( $debug )); then
          echo "File ${bin_path}${bin_file} already exists, skipping.."
        fi
        continue
      fi
    fi

    declare type=""
    if [[ "${ext,,}" = "vs" ]]; then
      type="vertex"
    elif [[ "${ext,,}" = "fs" ]]; then
      type="fragment"
    elif [[ "${ext,,}" = "gs" ]]; then
      type="geometry"
    fi
    if (( $debug )); then
      echo type=${type}
    fi

    echo "${src_path}${src_file} -> ${bin_path}${bin_file}"
    ${glslc_bin} -fshader-stage=$type "${src_path}${src_file}" -o "${bin_path}${bin_file}"
  done

else 
  echo "shaderc not found at ${glslc_bin} - Download shaderc, compile it, and place in project root ./"
  echo "https://github.com/google/shaderc"
fi

