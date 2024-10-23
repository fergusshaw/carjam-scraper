#include "scraperUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>

size_t curl_to_string(void *ptr, size_t size, size_t nmemb, void *data){

    if(size * nmemb == 0){
        return 0;
    }
    
    data_holder *vec = (data_holder *) data;

    //resize the data array if needed (+1 for null terminator)
    if(vec->size + size * nmemb + 1  > vec->allocated)
    {
        char *new_data = realloc(vec->data, sizeof(char) * (vec->size + size * nmemb) + 1);
        if(!new_data){
            return 0;
	}
        vec->data = new_data;
        vec->allocated = vec->size + size * nmemb + 1;
    }

    //add new data (ptr) to data in vec and update size
    memcpy(vec->data + vec->size, ptr, size * nmemb);
    vec->size += size * nmemb;

    vec->data[vec->size] = '\0';

    return size * nmemb;
}

int clear_data(data_holder *data) {
    if (!data || !data->data) {
        return -1; // Indicate an error if the input pointer is null.
    }

    // Free the current data if it's allocated
    free(data->data);

    // Allocate new memory
    data->data = (char*) malloc(50 * sizeof(char));
    if (!data->data) {
        return -1; // Indicate an error if memory allocation fails.
    }

    memset(data->data, '\0', 50);

    // Reset the size and allocated fields
    data->allocated = 50;
    data->size = 0;

    return 0;
}


bool curl_to_the_perform(CURL *handle_mate){
   CURLcode result;
   result = curl_easy_perform(handle_mate);
  
    if (result != CURLE_OK){
      fprintf(stderr, "perform thing failed: %s\n", curl_easy_strerror(result));
      return false;
    }
    else{
      return true;
    }
}

//converts unix time into dd/mm/yyyy format
char *time_calc(char *last_date){ 
  time_t timestamp = atoi(last_date);
  struct tm *local_time = localtime(&timestamp);
  int year = local_time->tm_year + 1900;
  int month = local_time->tm_mon + 1;
  int day = local_time->tm_mday;
  char *result = malloc(15);
  sprintf(result, "%02d-%02d-%04d", day, month, year);
  return result;
}


void curl_get_data(char *carjam_url, bool carjam){
  //initialize curl variables. 
  CURL *curl_handle;
 
  //initial allocation of where data will be stored. 
  data_holder *data = malloc(sizeof(data_holder));

  if (data == NULL){
    printf("haha data didnt allocate, loser");
    free(data);
    return;
  }
  
  data->size = 0;
  data->allocated=0;
  data->data = malloc(50);

  if (data->data == NULL){
    printf("haha data->data didnt allocate, loser");
    free(data->data);
    return;
  }

  //initialize libcurl functionality globally. ALL for all known sub modules
  curl_global_init(CURL_GLOBAL_ALL);
  //create a handle. (a logic entity for a transfer or set of transfers.)
  curl_handle = curl_easy_init();    

  if (curl_handle){
    
    //set url to recieve data from
    curl_easy_setopt(curl_handle, CURLOPT_URL, carjam_url);

    //it breaks without this and i'm not actually sure why
    data_holder vec = {0};
    
    //tell libcurl to pass data to the write function defined above. 
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_to_string);
    //tells curl to write the actual data
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, data);

    get_carjam_data(curl_handle, data, carjam_url);

    //free stuff and clean up.    
    curl_easy_cleanup(curl_handle);

  }

  free(data->data);
  free(data);
  curl_global_cleanup();
}
