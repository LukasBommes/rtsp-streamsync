FROM ubuntu:18.04

ENV HOME "/home"

RUN apt-get update && \
  apt-get install -y \
    git && \
    rm -rf /var/lib/apt/lists/*

###############################################################################
#
#							mv-extractor library (legacy version)
#
###############################################################################

# Build h264-videocap from source
RUN cd $HOME && \
  git clone https://github.com/LukasBommes/mv-extractor-legacy.git video_cap && \
  cd video_cap && \
  chmod +x install.sh && \
  ./install.sh

# Set environment variables
ENV PATH "$PATH:$HOME/bin"
ENV PKG_CONFIG_PATH "$PKG_CONFIG_PATH:$HOME/ffmpeg_build/lib/pkgconfig"

RUN cd $HOME/video_cap && \
  python3 setup.py install

###############################################################################
#
#							Python stream-sync module
#
###############################################################################

WORKDIR $HOME/stream_sync

COPY setup.py $HOME/stream_sync
COPY src $HOME/stream_sync/src/

# Install stream_sync Python module
RUN cd /home/stream_sync && \
  python3 setup.py install

WORKDIR $HOME

CMD ["sh", "-c", "tail -f /dev/null"]
