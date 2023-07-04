
all: docker

include Makefile.docker

DOCKER_TAG=some-cool-name-here
BUILD_TAG=some-build-name-here
DOCKER_BUILD_COMMAND=docker build -t $(DOCKER_TAG) .
DOCKER_BUILD_ENV_COMMAND=docker build -f teams/Dockerfile -t $(BUILD_TAG) .
IP=$(shell cat ip.txt)
DOCKER_RUN_COMMAND=docker run --env DISPLAY=$(IP):0 $(DOCKER_TAG)
DOCKER_BUILD_TEAM_COMMAND=docker run -v .:/root/teams/team $(BUILD_TAG)

TEAMS=$(filter-out teams/./ teams/../,$(sort $(dir $(wildcard teams/*/*) $(wildcard teams/.*/*))))
TEAM_FILES=$(addsuffix team.so,$(TEAMS))

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

docker: ip Dockerfile teams/Dockerfile $(TEST_FILES) $(BACKEND_FILES) $(FRONTEND_FILES) $(TEAM_FILES)
	@$(DOCKER_BUILD_COMMAND)
	@$(DOCKER_BUILD_ENV_COMMAND)
	@touch docker

$(TEAM_FILES): $(TEAMS)

$(TEAMS):
	@cd $@ && $(DOCKER_BUILD_TEAM_COMMAND)
	@chmod 777 $@team.so

run: docker
	@git pull
	@$(DOCKER_RUN_COMMAND)

.PHONY: run $(TEAMS)
