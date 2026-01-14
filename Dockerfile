FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
	build-essential \
	make \
	gcc \
	libcap2-bin \
	iproute2 \
	iputils-ping \
	tcpdump \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["bash"]
