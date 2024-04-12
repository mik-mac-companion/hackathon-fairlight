FROM mcr.microsoft.com/dotnet/core/sdk:3.1 AS build
WORKDIR /source

COPY GSDKWrapper/* ./
RUN dotnet publish -c release -o /wrapper -r linux-x64 -p:PublishSingleFile=true --self-contained true

FROM ubuntu:18.04

RUN useradd --system ue
USER ue

EXPOSE 7777/udp

WORKDIR /server

COPY --chown=ue:ue --from=build /wrapper .
COPY --chown=ue:ue ./Server/ ./
COPY --chown=ue:ue ./Start.sh .

ENV DOTNET_SYSTEM_GLOBALIZATION_INVARIANT 1

USER root
CMD ["sh", "/server/Start.sh"]
