FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV QT_VERSION=6

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    qt6-base-dev \
    libqt6sql6-sqlite \
    libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON
RUN cmake --build build --target all -j$(nproc)
RUN cd build && ctest --output-on-failure

FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    qt6-base-dev \
    libqt6sql6-sqlite \
    libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/src/client/motor_monitor_client /app/motor_monitor_client
COPY --from=builder /app/build/src/simulator/motor_simulator /app/motor_simulator

WORKDIR /app
ENV QT_QPA_PLATFORM=offscreen

CMD ["./motor_monitor_client"]