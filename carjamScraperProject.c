#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <curl/curl.h>

#define CARJAM_PREFIX "https://www.carjam.co.nz/car/?plate="

typedef struct
{
    size_t size;
    size_t allocated;
    char *data;
} data_holder;

char* get_url(char* user_input);
void curl_get_data(char *input);
int clear_data(data_holder *data);
char *time_calc(char *last_date);
bool is_loading_screen(data_holder *page_data);
char *extract_feature(char *data, char *divider, int optional_offset, int max_length);
void deal_with_the_data(data_holder *data, char *carjam_url);
size_t curl_to_string(void *ptr, size_t size, size_t nmemb, void *data);
bool curl_to_the_perform(CURL *handle_mate);


size_t curl_to_string(void *ptr, size_t size, size_t nmemb, void *data)
{
    if(size * nmemb == 0){
        return 0;
    }
    
    data_holder *vec = (data_holder *) data;

    //resize the data array if needed
    if(vec->size + size * nmemb > vec->allocated)
    {
        char *new_data = realloc(vec->data, sizeof(char) * (vec->size + size * nmemb));
        if(!new_data){
            return 0;
	}
        vec->data = new_data;
        vec->allocated = vec->size + size * nmemb + 1;
    }

    //add new data (ptr) to data in vec and update size
    memcpy(vec->data + vec->size, ptr, size * nmemb);
    vec->size += size * nmemb;

    return size * nmemb;
}

char* get_url(char* user_input){

  //getplatelengthbecausetheycanbelessthan6etc
  size_t plate_num_length = strlen(user_input);

  //simple input sanitation
  if ( plate_num_length > 6 || plate_num_length < 1 ){
    printf("Invalid plate entered! returning.\n");
    return NULL;
  }

  size_t url_length = strlen(CARJAM_PREFIX) + plate_num_length + 1;
  //allocate memory plus space for null terminator!!
  char* url = (char *)malloc(url_length);

  if (url == NULL){
    printf("memory allocation fucked out.\n");
    return NULL;
  }

  //create url string/char array with snprintf
  snprintf(url, url_length, "%s%s", CARJAM_PREFIX, user_input);
  
  return url; 
  
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

bool is_loading_screen(data_holder *page_data){
  //if data contains carjam loading screen text, we're probably in a loading screen
  return strstr(page_data->data, "Waiting for a few more things to happen...") != NULL;   
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

void curl_get_data(char *carjam_url){
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

    //do the thing
    if (curl_to_the_perform(curl_handle)){
      int iter = 0;
      while (is_loading_screen(data) && iter <6){
	
	printf("Loading screen detected. waiting 5 seconds and trying again...\n");

	sleep(5);
	clear_data(data);
	
	curl_to_the_perform(curl_handle);
	iter++;
      }
      deal_with_the_data(data, carjam_url); 
    }

    //free stuff and clean up.    
    curl_easy_cleanup(curl_handle);

  }

  free(data->data);
  free(data);
  curl_global_cleanup();
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

//this is a nice function. 
char *extract_feature(char *data, char *divider, int optional_offset, int max_length){
  
  char *FEATURE = malloc(max_length);

  if (data == NULL){
    strncpy(FEATURE, "Not found", 10);  
    return FEATURE;
  }
  
  data = data + optional_offset;
  
  //find divider in data, and calculate length of feature
  char *this_div = strstr(data, divider);
  int length = this_div - data;

  if (length > max_length){
    strncpy(FEATURE, "Not found", 10);
    return FEATURE;
  }
 
  strncpy(FEATURE, data, length);

  FEATURE[length] = '\0';

  return FEATURE;
}

void deal_with_the_data(data_holder *data, char *carjam_url){
  
  //create pointer for last reference, so we aren't searching from the beginning of the html everytime
  char *LAST_REFERENCE = malloc(100);
  
  //find beginning of part we want to look through
  char *REPORT_LOC = strstr(data->data, "<title>Report - ");
  
  //get each feature sequentially based on last reference, length and divider
  char *PLATE_NUM = extract_feature(REPORT_LOC + 16, " - ", 0, 7);
  LAST_REFERENCE = REPORT_LOC + 16 + strlen(PLATE_NUM);
  
  char *YEAR = extract_feature(LAST_REFERENCE + 3, " ", 0, 5);
  LAST_REFERENCE += strlen(YEAR) + 3;
  
  char *MAKE = extract_feature(LAST_REFERENCE + 1, " ", 0, 20); 
  LAST_REFERENCE += strlen(MAKE) + 1;
  
  char *MODEL = extract_feature(LAST_REFERENCE + 1, " ", 0, 20);
  LAST_REFERENCE += strlen(MODEL) + 1;
  
  char *COLOUR = extract_feature(LAST_REFERENCE + 4, " | ", 0, 15);
  LAST_REFERENCE += strlen(COLOUR) + 4;
  
  //everything after colour might not be there, so last reference isn't updated
  char *VIN = extract_feature(strstr(LAST_REFERENCE, "vin\":\""), "\",", 6, 30); 
  
  char *CHASSIS = extract_feature(strstr(LAST_REFERENCE, "chassis\":\""), "-", 10, 20);
  
  char *LAST_ODO_DATE_UNIX = extract_feature(strstr(LAST_REFERENCE, "odometer_date\":"), ",", 15, 30);
  char *LAST_ODO_DATE = (strstr(LAST_ODO_DATE_UNIX, "Not found") != NULL) ? strdup("Not found") : time_calc(LAST_ODO_DATE_UNIX);
  
  char *LAST_ODO_READING = extract_feature(strstr(LAST_REFERENCE, "odometer_reading\":\""), "\",", 19, 15);

  //73 after second repeat
  char *SUBMODEL = extract_feature(strstr(LAST_REFERENCE, "Submodel:"), "</span", 85, 15);
  SUBMODEL = (isupper(SUBMODEL[0])) ? SUBMODEL : strdup("Not found");
  
  char *BODY_STYLE = extract_feature(strstr(LAST_REFERENCE, "Body Style:"), "</span", 40, 10);
  
  //display results nicely
  printf("Plate number: %s\nYear: %s\nMake: %s\nModel: %s\nSubmodel: %s\nBody Style: %s\nColour: %s\nLast public odometer reading: %s on %s\nVIN: %s\nChassis: %s\n",
	 PLATE_NUM, YEAR, MAKE, MODEL, SUBMODEL, BODY_STYLE, COLOUR, LAST_ODO_READING, LAST_ODO_DATE, VIN, CHASSIS);
  
  //be free
  free(PLATE_NUM);
  free(YEAR);
  free(MAKE);
  free(MODEL);
  free(COLOUR);
  free(LAST_ODO_DATE_UNIX);
  free(SUBMODEL);
  free(BODY_STYLE);
  free(LAST_ODO_READING);
  free(LAST_ODO_DATE);
  free(VIN);
  free(CHASSIS);
  }

int main(int argc, char* argv[]){
  
  //if no argument return and complain
  if (argv[1] == NULL){
    printf("No plate entered. returning\n");
    return -1;
  }
  
  char* carjam_url = get_url(argv[1]); 
  
  if (carjam_url == NULL){
    printf("url getting failed. sort it out\n");
    return -1;
  }

  curl_get_data(carjam_url);

  free(carjam_url);
  
  return 0;
  
}
