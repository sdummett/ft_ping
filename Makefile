NAME        := ft_ping

SRCS_DIR    := srcs
OBJS_DIR    := objs
INC_DIR     := incs

CC          := cc
CFLAGS      := -Wall -Wextra -Werror -lm -I$(INC_DIR)

SRCS        := $(wildcard $(SRCS_DIR)/*.c)
OBJS        := $(SRCS:$(SRCS_DIR)/%.c=$(OBJS_DIR)/%.o)
HEADERS     := $(wildcard $(INC_DIR)/*.h)

all: $(NAME)

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.c $(HEADERS) | $(OBJS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	sudo setcap cap_net_raw+ep $@

clean:
	rm -rf $(OBJS_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

debug: CFLAGS += -g3 -fsanitize=address,undefined
debug: re

.PHONY: all clean fclean re debug
