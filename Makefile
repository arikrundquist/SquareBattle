
all: docker

include Makefile.docker

DOCKER_TAG=some-cool-name-here
DOCKER_BUILD_COMMAND=docker build -t $(DOCKER_TAG) .
IP=$(shell cat ip.txt)
DOCKER_RUN_COMMAND=docker run --env DISPLAY=$(IP):0 $(DOCKER_TAG)

PAUSE_COMMAND=read -p "press enter to continue..."

ip.txt: getting-started.txt
	@cat getting-started.txt
	@touch ip.txt
	@echo docker will be built with: $(DOCKER_BUILD_COMMAND)
	@$(PAUSE_COMMAND)

ip: ip.txt
	@touch ip
	@echo ip set to: "$(IP)"
	@echo docker will be run with: $(DOCKER_RUN_COMMAND)
	@$(PAUSE_COMMAND)

docker: ip Dockerfile $(TEST_FILES) $(BACKEND_FILES) $(FRONTEND_FILES)
	@$(DOCKER_BUILD_COMMAND)
	@touch docker

run: docker
	@$(DOCKER_RUN_COMMAND)

.PHONY: run
