# Prisma FX - SPDX-License-Identifier: BSD-3-Clause
#
# Windows: turns the generated PiPL source into a .rc file, the way the AE
# SDK samples do it (preprocess, PiPLtool, preprocess again).
# cmake -DCL=cl.exe -DPIPLTOOL=PiPLtool.exe -DHEADERS=... -DIN=x.r -DOUT=x.rc -P AePiPL.cmake
get_filename_component(dir "${OUT}" DIRECTORY)
execute_process(COMMAND "${CL}" /nologo /I "${HEADERS}" /D MSWindows /EP /Tc "${IN}"
                OUTPUT_FILE "${dir}/PrismaFX_PiPL.rr" RESULT_VARIABLE rc1)
if(rc1)
  message(FATAL_ERROR "Preprocessing ${IN} failed")
endif()
execute_process(COMMAND "${PIPLTOOL}" "${dir}/PrismaFX_PiPL.rr" "${dir}/PrismaFX_PiPL.rrc" RESULT_VARIABLE rc2)
if(rc2)
  message(FATAL_ERROR "PiPLtool failed")
endif()
execute_process(COMMAND "${CL}" /nologo /D MSWindows /EP /Tc "${dir}/PrismaFX_PiPL.rrc"
                OUTPUT_FILE "${OUT}" RESULT_VARIABLE rc3)
if(rc3)
  message(FATAL_ERROR "Preprocessing the PiPL .rrc failed")
endif()
