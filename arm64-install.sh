
#!/bin/bash

# copy shared libraries to /usr/lib

sudo cp -r binaries/arm64/lib/ /usr/lib

# copy header files to /usr/include create folder if not exist

sudo mkdir -p /usr/include

sudo cp -r binaries/arm64/include/* /usr/include

echo "Done"