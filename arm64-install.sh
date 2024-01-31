
#!/bin/bash

# copy shared libraries to /usr/lib

sudo cp -r binaries/arm64/lib/ /usr/lib

# copy header files to /usr/include create folder if not exist

sudo mkdir -p /usr/include

sudo cp -r binaries/arm64/include/* /usr/include

# make required symbolic links

sudo ln -s /usr/lib/librnnoise.so.0.4.1 /usr/lib/librnnoise.so.0
sudo ln -s /usr/lib/librnnoise.so.0.4.1 /usr/lib/librnnoise.so

echo "Done"