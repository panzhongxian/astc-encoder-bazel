function install_deps() {
  WORKSPACE_DIR=$PWD/../..
  BASE_DIR=$PWD
  BUILD_DIR=$PWD/build

  # install bazel
  BAZEL_VERSION=`bazel version | grep label | awk '{print $NF}'`
  if [ -z "$BAZEL_VERSION" ]; then
	BAZEL_VERSION=3.7.0
	BAZEL_INSTALLER=bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh
	wget https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/$BAZEL_INSTALLER && sh $BAZEL_INSTALLER && rm -rf $BAZEL_INSTALLER 
  fi

  cd $WORKSPACE_DIR
  bazel build //...
  bazel build @astc-encoder//...

  # only for module style
  cd python
  python3 setup.py install

  mkdir -p $BUILD_DIR && cd $BUILD_DIR
  python3 -m pip install flask pyinstaller
  cp ${WORKSPACE_DIR}/bazel-bin/src/libastc_wrapper.so ${WORKSPACE_DIR}/bazel-bin/external/astc-encoder/libastc-encoder.so ./
  #cp -L `ldd libastc-encoder.so | grep libstdc++.so.6 | awk '{print $3}'` ./
  cp -L `ldconfig -p | grep libstdc++.so.6 | head -1 | awk '{print $NF}'` ./
  pyinstaller --onefile --clean ../astc-server.py
  mv dist/astc-server ./
  tar -czf python-http-server-dist.tar.gz astc-server libastc_wrapper.so libastc-encoder.so libstdc++.so.6 
  mv python-http-server-dist.tar.gz $BASE_DIR
  rm -rf $BUILD_DIR
}


function start() {
  if test -f libastc_wrapper.so; then
	echo "dependencies are installed."
  else
	echo "installing dependencies..."
	install_deps
  fi
  ./astc-server-module
}

if [[ $2 == 'start' ]]; then
  start
else
  install_deps
fi
