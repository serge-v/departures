extern const char credits[];
extern const size_t sz_credits;
extern const char *stations[];
extern const char *codes[];

void stations_list();
size_t station_index(const char *code);
const char *station_name(const char *code);
const char *station_code(const char *name);
const char *station_verify_code(const char *code);

