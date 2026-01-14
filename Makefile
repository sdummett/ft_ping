NAME        := ft_ping

SRCS_DIR    := srcs
OBJS_DIR    := objs
INC_DIR     := incs

CC          := cc
CFLAGS      := -Wall -Wextra -Werror -I$(INC_DIR)
LDLIBS      := -lm

SRCS        := $(wildcard $(SRCS_DIR)/*.c)
OBJS        := $(SRCS:$(SRCS_DIR)/%.c=$(OBJS_DIR)/%.o)
HEADERS     := $(wildcard $(INC_DIR)/*.h)

all: $(NAME)

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.c $(HEADERS) | $(OBJS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@setcap cap_net_raw+ep $@ 2>/dev/null || echo "Info: setcap skipped. You can use Docker with cap_add: NET_RAW."

clean:
	rm -rf $(OBJS_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

debug: CFLAGS += -g3 -fsanitize=address,undefined
debug: re

DOCKER_COMPOSE ?= docker compose
DOCKER_SERVICE ?= ft_ping

docker-build:
	$(DOCKER_COMPOSE) build $(DOCKER_SERVICE)

# Persistent container (must not be recreated on every call)
docker-up:
	$(DOCKER_COMPOSE) up -d --no-recreate $(DOCKER_SERVICE)

docker-down:
	$(DOCKER_COMPOSE) down

# Enter the same container
# - If the container does not exist: build + up
# - If it already exists: exec without rebuild/up that would recreate it (otherwise it kills the first shell)
docker-enter:
	@CID="$$( $(DOCKER_COMPOSE) ps -q $(DOCKER_SERVICE) 2>/dev/null )"; \
	if [ -z "$$CID" ]; then \
		$(DOCKER_COMPOSE) build $(DOCKER_SERVICE); \
		$(DOCKER_COMPOSE) up -d --no-recreate $(DOCKER_SERVICE); \
	else \
		RUNNING="$$(docker inspect -f '{{.State.Running}}' $$CID 2>/dev/null || echo false)"; \
		if [ "$$RUNNING" != "true" ]; then \
			$(DOCKER_COMPOSE) up -d --no-recreate $(DOCKER_SERVICE); \
		fi; \
	fi; \
	$(DOCKER_COMPOSE) exec $(DOCKER_SERVICE) bash

.PHONY: all clean fclean re debug docker-build docker-up docker-down docker-enter
