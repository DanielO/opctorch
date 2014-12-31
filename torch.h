void	default_conf(struct config_t *);
int	ini2conf(dictionary *, struct config_t *);
int	create_torch(int, struct config_t *);
int	run_torch(struct config_t *);
void	free_torch(void);
void	cmd_torch(const char *, int, char **);
void	newMessage(struct config_t *, char *);
