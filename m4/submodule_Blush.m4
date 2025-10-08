
AC_DEFUN([submodule_Blush], [
	# Blush for 3D refinement
	AC_ARG_ENABLE(blush, AS_HELP_STRING([--enable-blush],[Compile with LibTorch for 3D refinement [default=no]]),
	[enable_blush="$enableval"],
	[enable_blush="no"]
	)


	AC_ARG_WITH([libtorch], AS_HELP_STRING([--with-libtorch=DIR],[Path to LibTorch directory containing libs to run blush refinement (default: system)]),
	[libtorch_path="$withval"],
	[libtorch_path=""] 
	)

	# Fill in with the default devcontainer LibTorch path if the user specifies no path
	if test "x$libtorch_path" = "x"; then
		libtorch_path="/opt/libtorch"
	fi

	BLUSH_CPPFLAGS=""
	BLUSH_LDFLAGS=""
	BLUSH_LIBS=""

	if test "x$enable_blush" = "xyes"; then
		AC_DEFINE([cisTEM_USING_BLUSH], [1], [Define the Blush flag])
	

		if test "x$libtorch_path" != "x"; then
			BLUSH_CPPFLAGS="-I$libtorch_path/include/torch/csrc/api/include -I$libtorch_path/include"
			BLUSH_LDFLAGS="-L$libtorch_path/lib -Wl,-rpath,$libtorch_path/lib"
		fi
	
		AC_MSG_NOTICE([Using LibTorch path: $libtorch_path])
	
		# CPPFLAGS_SAVED="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $BLUSH_CPPFLAGS"
		CXXFLAGS="$CXXFLAGS -std=c++17"
	
		# NOTE: this will issue a warning even when torch.h is found, because autoconf still uses the C preprocessor for
		# checking headers, but we need to use C++ for LibTorch.
		AC_LANG_PUSH([C++])
		AC_CHECK_HEADERS([torch/torch.h], 
			[AC_MSG_NOTICE([Found LibTorch header])], 
			[AC_MSG_ERROR([LibTorch header not found. Please install LibTorch, disable blush, or specify the path with --with-libtorch])]
		)
		# CPPFLAGS="$CPPFLAGS_SAVED"
	
		AC_CHECK_FILES([$libtorch_path/lib/libtorch.so $libtorch_path/lib/libtorch_cpu.so $libtorch_path/lib/libc10.so],
			[AC_MSG_NOTICE([Found LibTorch library])],
			[AC_MSG_ERROR([LibTorch library not found. Please install LibTorch, disable blush, or specify the path with --with-libtorch])]
		)
		AC_LANG_POP([C++])
	
		#   BLUSH_LIBS="$BLUSH_LDFLAGS -Wl,--start-group $libtorch_path/lib/libtorch.a $libtorch_path/lib/libtorch_cpu.a $libtorch_path/lib/libc10.a $libtorch_path/lib64/libsleef.a $libtorch_path/lib64/libcpuinfo.a $libtorch_path/lib64/libfbgemm.a $libtorch_path/lib64/libasmjit.a $libtorch_path/lib64/libprotobuf.a $libtorch_path/lib64/libprotoc.a $libtorch_path/lib64/libclog.a $libtorch_path/lib64/libdnnl.a $libtorch_path/lib64/libfmt.a $libtorch_path/lib64/libittnotify.a $libtorch_path/lib64/libkineto.a $libtorch_path/lib64/libmicrokernels-prod.a $libtorch_path/lib64/libnnpack.a $libtorch_path/lib64/libprotobuf-lite.a $libtorch_path/lib64/libpthreadpool.a $libtorch_path/lib64/libpytorch_qnnpack.a $libtorch_path/lib64/libXNNPACK.a -Wl,--end-group"
		BLUSH_LIBS="$BLUSH_LDFLAGS -ltorch -ltorch_cpu -lc10 -pthread -lm -ldl"
	fi

	AC_SUBST([BLUSH_CPPFLAGS])
	AC_SUBST([BLUSH_LDFLAGS])
	AC_SUBST([BLUSH_LIBS])

	AM_CONDITIONAL([ENABLE_BLUSH], [test "x$enable_blush" = "xyes"])
])