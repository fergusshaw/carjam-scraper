#ifndef SCRAPER_UTILS_H
#define SCRAPER_UTILS_H

#include <stddef.h>
#include <stdbool.h>
#include <curl/curl.h>

typedef struct {
    size_t size;
    size_t allocated;
    char *data;
} data_holder;

int clear_data(data_holder *data);
size_t curl_to_string(void *ptr, size_t size, size_t nmemb, void *data);
bool curl_to_the_perform(CURL *handle_mate);
char *time_calc(char *last_date);
void curl_get_data(char *carjam_url, bool carjam);
void get_carjam_data(CURL *curl_handle, data_holder *data, char *carjam_url);


#endif
