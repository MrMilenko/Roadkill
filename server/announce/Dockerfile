# Roadkill's announce / rendezvous server: one static binary on UDP 27400.
#   docker build -t roadkill-announce server/announce
#   docker run -d --name roadkill-announce --restart unless-stopped -p 27400:27400/udp roadkill-announce
FROM alpine:3.20 AS build
RUN apk add --no-cache gcc musl-dev
COPY announce.c announce_proto.h /src/
RUN gcc -O2 -Wall -Wextra -static -o /announce /src/announce.c

FROM scratch
COPY --from=build /announce /announce
USER 65534:65534
EXPOSE 27400/udp
ENTRYPOINT ["/announce"]
