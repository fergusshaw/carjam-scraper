#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <curl/curl.h>
#include "scraperUtils.h"

#define CARJAM_PREFIX "https://www.carjam.co.nz/car/?plate="
#define FEATURE_NAME(feature) #feature

char* get_url(char* user_input);
bool is_loading_screen(data_holder *page_data);
char *extract_feature(char *data, char *divider, int optional_offset, int max_length);
void deal_with_the_data(data_holder *data, char *carjam_url);
void check_for_failure(data_holder *page_data);
void get_carjam_data(CURL *curl_handle, data_holder *data, char *carjam_url);

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
    printf("memory allocation failed.\n");
    return NULL;
  }

  //create url string/char array with snprintf
  snprintf(url, url_length, "%s%s", CARJAM_PREFIX, user_input);

  return url;

}

bool is_loading_screen(data_holder *page_data){
  //if data contains carjam loading screen text, we're probably in a loading screen
  return strstr(page_data->data, "Waiting for a few more things to happen...") != NULL;
}

void check_for_failure(data_holder *page_data){
  //if back at the home screen, plate was not found
  if (strlen(page_data->data) == 0){
    printf("No car was found. Did you get the plate right?\n");
    exit(0);
  }
}

void get_carjam_data(CURL *curl_handle, data_holder *data, char *carjam_url){
    //do the thing
    if (curl_to_the_perform(curl_handle)){
      while (is_loading_screen(data)){

        printf("Loading screen detected. Waiting 5 seconds and trying again...\n");
        sleep(5);
        clear_data(data);

        curl_to_the_perform(curl_handle);
      }

      check_for_failure(data);
      deal_with_the_data(data, carjam_url);
    }

}

//this is a nice function.
char *extract_feature(char *data, char *divider, int optional_offset, int max_length){

  char *feature = malloc(max_length);

  if (data == NULL){
    strncpy(feature, "Not found", 10);
    return feature;
  }

  data += optional_offset;

  //find divider in data, and calculate length of feature
  char *this_div = strstr(data, divider);
  int length = this_div - data;

  if (length > max_length){
    strncpy(feature, "Not found", 10);
    return feature;
  }

  strncpy(feature, data, length);

  feature[length] = '\0';

  return feature;
}

void deal_with_the_data(data_holder *data, char *carjam_url){

  //create pointer for last reference, so we aren't searching from the beginning of the html everytime
  char *last_reference;

  //find beginning of part we want to look through
  char *report_loc = strstr(data->data, "<title>Report - ");

  //get each feature sequentially based on last reference, length and divider
  char *plate_number = extract_feature(report_loc + 16, " - ", 0, 7);
  last_reference = report_loc + 16 + strlen(plate_number);

  char *year = extract_feature(last_reference + 3, " ", 0, 5);
  last_reference += strlen(year) + 3;

  char *make = extract_feature(last_reference + 1, " ", 0, 20);
  last_reference += strlen(make) + 1;

  char *model = extract_feature(last_reference + 1, " ", 0, 20);
  last_reference += strlen(model) + 1;

  char *colour = extract_feature(last_reference + 4, " | ", 0, 15);
  last_reference += strlen(colour) + 4;

  //everything after colour might not be there, so last reference isn't updated
  char *vin = extract_feature(strstr(last_reference, "vin\":\""), "\",", 6, 30);

  char *chassis = extract_feature(strstr(last_reference, "chassis\":\""), "-", 10, 20);

  char *last_odo_date_unix = extract_feature(strstr(last_reference, "odometer_date\":"), ",", 15, 30);
  char *last_odo_date = (strstr(last_odo_date_unix, "Not found") != NULL) ? strdup("Not found") : time_calc(last_odo_date_unix);

  char *last_odo_reading = extract_feature(strstr(last_reference, "odometer_reading\":\""), "\",", 19, 15);

  char *grade = extract_feature(strstr(last_reference, "grade\":"), "\",", 8, 20);

  char *manufacture_date = extract_feature(strstr(last_reference, "manufacture_date\":"), "\",", 19, 10);

  char *engine = extract_feature(strstr(last_reference, "engine\":"), "\",", 9, 8);

  char *drive = extract_feature(strstr(last_reference, "drive\":"), "\",", 8, 10);

  char *transmission = extract_feature(strstr(last_reference, "transmission\":"), "\",", 15, 5);

  char *submodel = extract_feature(strstr(last_reference, "Submodel:"), "</span", 38, 25);

  char *body_style = extract_feature(strstr(last_reference, "Body Style:"), "</span", 40, 20);

  char *engine_size = extract_feature(strstr(last_reference, "CC rating:"), " <span", 622, 8);

  char * all_the_data[] = {plate_number, year, make, model, submodel, grade, manufacture_date, engine, engine_size, drive, transmission,
      body_style, colour, vin, chassis};

  char * all_the_names[] = {"Plate number", "Year", "Make", "Model", "Submodel", "Grade", "Manufacture date", "Engine", "Engine size", "Drive",
      "Transmission", "Body style", "Colour","Vin", "Chassis"};

  //print everthing nicely, only the values that were found
  for (int index = 0; index < (sizeof(all_the_data) / sizeof(char*)); index++){
      if (strstr(all_the_data[index], "Not found") != NULL){ continue; }

      printf("%s: %s\n", all_the_names[index], all_the_data[index]);
  }

  printf("Last public odometer reading: %s on %s\n", last_odo_reading, last_odo_date);

  //be free
  free(plate_number);
  free(year);
  free(make);
  free(model);
  free(colour);
  free(last_odo_date_unix);
  free(submodel);
  free(grade);
  free(manufacture_date);
  free(engine);
  free(engine_size);
  free(drive);
  free(transmission);
  free(body_style);
  free(last_odo_reading);
  free(last_odo_date);
  free(vin);
  free(chassis);
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

  curl_get_data(carjam_url, true);

  free(carjam_url);

  return 0;

}
