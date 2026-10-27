FROM node:22-bookworm-slim AS node
FROM ubuntu:24.04
COPY --from=node /usr/local/bin/node /usr/local/bin/node
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential clang lld cmake ninja-build git curl ca-certificates python3 python3-venv \
    bison flex gperf pkg-config patchelf binutils unzip xz-utils \
    libssl-dev libsecret-1-dev gnome-keyring dbus-x11 xvfb xauth \
    libavcodec-dev libavformat-dev libavutil-dev libnss3-dev libnspr4-dev \
    libasound2-dev libpulse-dev libdbus-1-dev libudev-dev libdrm-dev libgbm-dev \
    libegl1-mesa-dev libgl1-mesa-dev libopengl-dev libxkbcommon-dev libxkbcommon-x11-dev \
    libxcomposite-dev libxcursor-dev libxdamage-dev libxext-dev libxfixes-dev \
    libxi-dev libxrandr-dev libxrender-dev libxss-dev libxtst-dev libxshmfence-dev \
    libxcb-cursor-dev libxcb-icccm4-dev libxcb-keysyms1-dev libxcb-shape0-dev \
    libxcb-image0-dev libxcb-render-util0-dev libxcb-xinerama0-dev libxcb-randr0-dev libxcb-dri3-dev libxkbfile-dev \
    libfontconfig1-dev libfreetype-dev libcups2-dev libcap-dev libpci-dev \
    libjpeg-dev libpng-dev libtiff-dev libwebp-dev libxml2-dev libxslt1-dev \
    libsnappy-dev zlib1g-dev fonts-liberation && rm -rf /var/lib/apt/lists/*
COPY dependencies.json manifest.json /opt/arbiter-pins/
RUN python3 -m venv /opt/python && /opt/python/bin/pip install --no-cache-dir aqtinstall==3.3.0 \
    && /opt/python/bin/python -c 'import json,subprocess; p=json.load(open("/opt/arbiter-pins/dependencies.json")); subprocess.run(["/opt/python/bin/pip","install","html5lib=="+p["html5lib"]],check=True)'
ENV PATH=/opt/python/bin:/opt/rust/bin:$PATH RUSTUP_HOME=/opt/rustup CARGO_HOME=/opt/rust
ARG ARBITER_BUILD_JOBS=24
ARG ARBITER_EXTERNAL_QT=false
COPY provision-qt.py /opt/arbiter-pins/
RUN if [ "$ARBITER_EXTERNAL_QT" != true ]; then python /opt/arbiter-pins/provision-qt.py; fi
# System FFmpeg requires system Opus as well. Keep this layer after the costly Qt provisioning.
RUN apt-get update && apt-get install -y --no-install-recommends libopus-dev && rm -rf /var/lib/apt/lists/*
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs -o /tmp/rustup.sh \
    && sh /tmp/rustup.sh -y --profile minimal --default-toolchain none \
    && python -c 'import json,subprocess; subprocess.run(["rustup","toolchain","install",json.load(open("/opt/arbiter-pins/dependencies.json"))["rust"],"--profile","minimal"],check=True)' \
    && chmod -R a+rX /opt/rust /opt/rustup && rm /tmp/rustup.sh
# Run Chromium tests unprivileged with its sandbox enabled.
RUN mkdir -p /work /artifacts && chmod 1777 /work /artifacts
# D-Bus/Secret Service need a passwd entry for the non-root numeric build user.
ARG ARBITER_BUILD_UID=1000
ARG ARBITER_BUILD_GID=1000
RUN (getent group "$ARBITER_BUILD_GID" >/dev/null || groupadd --gid "$ARBITER_BUILD_GID" arbiter-builder) \
    && (getent passwd "$ARBITER_BUILD_UID" >/dev/null || useradd --uid "$ARBITER_BUILD_UID" --gid "$ARBITER_BUILD_GID" --home-dir /work/home --no-create-home arbiter-builder)
COPY entrypoint.sh /usr/local/bin/arbiter-linux-build
ENTRYPOINT ["bash", "/usr/local/bin/arbiter-linux-build"]
