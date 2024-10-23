#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <curl/curl.h>
#include "scraperUtils.h"

#define CARJAM_PREFIX "https://www.carjam.co.nz/car/?plate="

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
    printf("No car was found. Did you get the plate right?");
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

  char *FEATURE = malloc(max_length);

  if (data == NULL){
    strncpy(FEATURE, "Not found", 10);
    return FEATURE;
  }

  data += optional_offset;

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
  char *LAST_REFERENCE;

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

  char *GRADE = extract_feature(strstr(LAST_REFERENCE, "grade\":"), "\",", 8, 20);

  char *MANUFACTURE_DATE = extract_feature(strstr(LAST_REFERENCE, "manufacture_date\":"), "\",", 19, 10);

  char *ENGINE = extract_feature(strstr(LAST_REFERENCE, "engine\":"), "\",", 9, 8);

  char *DRIVE = extract_feature(strstr(LAST_REFERENCE, "drive\":"), "\",", 8, 10);

  char *TRANSMISSION = extract_feature(strstr(LAST_REFERENCE, "transmission\":"), "\",", 15, 5);

  char *SUBMODEL = extract_feature(strstr(LAST_REFERENCE, "Submodel:"), "</span", 38, 25);

  char *BODY_STYLE = extract_feature(strstr(LAST_REFERENCE, "Body Style:"), "</span", 40, 20);

  char *ENGINE_SIZE = extract_feature(strstr(LAST_REFERENCE, "CC rating:"), " <span", 622, 8);

  //display results nicely
  printf("Plate number: %s\nYear: %s\nMake: %s\nModel: %s\nSubmodel: %s\nGrade: %s\nManufacture date: %s\nEngine: %s\nEngine size: %s\n"
      "Drive: %s\nTransmission: %s\nBody Style: %s\nColour: %s\nLast public odometer reading: %s on %s\nVIN: %s\nChassis: %s\n",
	 PLATE_NUM, YEAR, MAKE, MODEL, SUBMODEL, GRADE, MANUFACTURE_DATE, ENGINE, ENGINE_SIZE, DRIVE, TRANSMISSION, BODY_STYLE, COLOUR,
	 LAST_ODO_READING, LAST_ODO_DATE, VIN, CHASSIS);

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

  curl_get_data(carjam_url, true);

  free(carjam_url);

  return 0;

}
