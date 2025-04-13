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
char *get_plate_history(char *history_section);
char **get_most_of_it(data_holder *data);
char **window_jph_there(data_holder *data);
char **window_jph_not_there(data_holder *data);

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

  if (length > max_length || length == 0){
    strncpy(feature, "Not found", 10);
    return feature;
  }

  strncpy(feature, data, length);

  feature[length] = '\0';

  return feature;
}

char *get_plate_history(char *history_section) {
  //tables can get really big when there's been 10+ plates on a car
  char *whole_table = extract_feature(history_section, "</table>", 0, 2420);
  static char result_string[420];
  result_string[0] = '\0';  // Initialize empty string
  
  // Look for license plates in <td> tags
  char *position = whole_table;
  while ((position = strstr(position, "<td>")) != NULL) {
    position += 4;  // Move past "<td>"
      
    // Find the end of this cell
    char *end_td = strstr(position, "</td>");
    if (!end_td) break;
      
    // Calculate length of content
    int content_length = end_td - position;
    if (content_length > 0 && content_length < 7) {  // Reasonable length for plate
  
      // Copy plate to a temporary buffer
      char temp[7];
      int temp_idx = 0;
     
      for (int i = 0; i < content_length; i++) {
        //if (isalnum(position[i])) {
        temp[temp_idx++] = position[i];
        //}
      }
      temp[temp_idx] = '\0';
                
      // If we found a valid plate, add it to results
      if (temp_idx > 0) {
        strcat(result_string, "\n");
        strcat(result_string, temp);
      }
    }
    // Move to next position
    position = end_td;
  }
 
  return result_string;
}

void deal_with_the_data(data_holder *data, char *carjam_url){

  char **most_of_the_data = get_most_of_it(data);
  //plate history is [16]
  
  char *most_of_the_names[] = {"Plate number", "Year", "Make", "Model", "Colour", "Chassis", "Submodel", "Body Style", "Engine Size", "Fuel type", "Popularity", "Power", "Weight", "Country of origin"};


  int rest_size;
  char **rest_of_it;
  char *rest_of_the_names[6];

  bool window_jph_there_bool = (strstr(data->data, "window.jph_search") != NULL);
  if (window_jph_there_bool){
    rest_of_it = window_jph_there(data);
    
    //individually assign because we've already declared array
    rest_of_the_names[0] = "Grade";
    rest_of_the_names[1] = "Manufacture date";
    rest_of_the_names[2] = "Engine";
    rest_of_the_names[3] = "Drive";
    rest_of_the_names[4] = "Transmission";
    rest_of_the_names[5] = "VIN";

    rest_size = 6;
  }
  else{
    rest_of_it = window_jph_not_there(data);

    rest_of_the_names[0] = "VIN";
    rest_of_the_names[1] = "Engine";
    rest_of_the_names[2] = "Transmission";

    rest_size = 3;
  }
  

  //print everthing nicely, only the values that were found
  
  //only print up to the 13th element, we have other plans for odo date, reading and plate history
  for (int index = 0; index < 14; index++){
    if (strstr(most_of_the_data[index], "Not found") != NULL){ continue; }

    printf("%s: %s\n", most_of_the_names[index], most_of_the_data[index]);
    //free(most_of_the_data[index]);
  }

  
  for (int index = 0; index < rest_size; index++){
    //due to this being a hard coded web scraper, i overlooked something that 
    //means values can sometimes appear twice, in different variables. (usually here)
    //this loop filters out these duplicates :)
    int contains = 0;
    for (int idx = 0; idx < 14; idx++){
      if (strcmp(most_of_the_data[idx], rest_of_it[index]) == 0){ 
        contains = 1; 
        break;
      }
    }
    if (strstr(rest_of_it[index], "Not found") != NULL || contains == 1){ continue; }

    printf("%s: %s\n", rest_of_the_names[index], rest_of_it[index]);
    //free(rest_of_it[index]);
  }
  
  
  char *last_odo_reading = most_of_the_data[16];
  char *last_odo_date = most_of_the_data[15];
  if (strstr(last_odo_reading, "Not found") == NULL || strstr(last_odo_date, "Not found") == NULL){
    printf("Last public odometer reading: %s on %s\n", last_odo_reading, last_odo_date);
  }
  
  //make sure there is a plate
  //if plate less than two there will be a previous one since custom!!
  if (strlen(most_of_the_data[14]) > 2){
    printf("Plate history:%s\n", most_of_the_data[14]);
  }
  
  //free(last_odo_date);
  //free(last_odo_reading);

  free(most_of_the_data);
  free(rest_of_it);

 }

char **get_most_of_it(data_holder *data){
  //find beginning of part we want to look through
  char *search_from = strstr(data->data, "<title>Report - ");

  //get each feature sequentially based on search from point, length and divider
  char *plate_number = extract_feature(search_from + 16, " - ", 0, 7);
  search_from += 16 + strlen(plate_number);

  char *year = extract_feature(search_from + 3, " ", 0, 5);
  search_from += 7; //4 for year + 3

  char *make = extract_feature(search_from + 1, " ", 0, 20);
  search_from += strlen(make) + 1;

  char *model = extract_feature(search_from + 1, " in", 0, 20);
  search_from+= strlen(model) + 1;

  char *colour = extract_feature(search_from + 4, " | ", 0, 15);
  search_from+= strlen(colour) + 4;

  char *last_odo_date_unix = extract_feature(strstr(search_from, "odometer_date\":"), ",", 15, 30);
  char *last_odo_date = (strstr(last_odo_date_unix, "Not found") != NULL) ? strdup("Not found") : time_calc(last_odo_date_unix);

  char *last_odo_reading = extract_feature(strstr(search_from, "odometer_reading\":\""), "\",", 19, 15);

  char *chassis = extract_feature(strstr(search_from, "chassis\":\""), "-", 10, 20);

  char *submodel_section = strstr(search_from, "<span class=\"key\" data-key=\"submodel\">");
  char *submodel = (submodel_section == NULL) ? "Not found" : extract_feature(strstr(submodel_section, "value\">"), "<", 7, 30);

  char *body_style_section = strstr(search_from, "<span class=\"key\" data-key=\"body_style\">");
  char *body_style = (body_style_section == NULL) ? "Not found" : extract_feature(strstr(body_style_section, "value\">"), "</span", 7, 20);
  
  char *engine_size_section = strstr(search_from, "<span class=\"key\" data-key=\"cc_rating\">");
  char *engine_size = (engine_size_section == NULL) ? "Not found" : extract_feature(strstr(engine_size_section, "value\">"), " <span", 7, 8);

  char *fuel_type_section = strstr(search_from, "<span class=\"key\" data-key=\"fuel_type\">");
  char *fuel_type = (fuel_type_section == NULL) ? "Not found" : extract_feature(strstr(fuel_type_section, "value\">"), "</span>", 7, 10);

  char *popularity_section = strstr(search_from, "<span class=\"key\" data-key=\"popularity\">");
  char *popularity = (popularity_section == NULL) ? "Not found" : extract_feature(strstr(popularity_section, "href=\"/nz-fleet/\" >"), "</a>", 19, 40);

  char *country_section = strstr(search_from, "<span class=\"key\" data-key=\"country_of_origin\">");
  char *country = (country_section == NULL) ? "Not found" : extract_feature(strstr(country_section, "value\">"), "</span>", 16, 15);

  char *power_section = strstr(search_from, "<span class=\"key\" data-key=\"power\">");
  char *power = (power_section == NULL) ? "Not found" : extract_feature(strstr(power_section, "value\">"), "</span>", 7, 10);

  char *weight_section = strstr(search_from, "<span class=\"key\" data-key=\"gross_vehicle_mass\">");
  char *weight = (weight_section == NULL) ? "Not found" : extract_feature(strstr(weight_section, "value\">"), "</span", 7, 8);

  char *plate_history = get_plate_history(strstr(search_from, "<th class=\'key\'>Effective Date</th>"));

  char *all_the_data[] = {plate_number, year, make, model, colour, chassis, submodel, body_style, engine_size, fuel_type, popularity, power, weight, country, plate_history, last_odo_date, last_odo_reading};

  int size = 18; 

  char **all_data_returnable = malloc(size * sizeof(char *));
  if (!all_data_returnable){
    printf("No good. all data returnable alloc failed");
    return NULL;
  }

  for (int feature = 0; feature < 17; feature++){
    all_data_returnable[feature] = all_the_data[feature];
  }
  all_data_returnable[17] = NULL;

  return all_data_returnable;

}

char **window_jph_not_there(data_holder *data){

  char *search_from = strstr(data->data, "table table-condensed no-border\">");
  
  char *vin_section = strstr(search_from, "<span class=\"key\" data-key=\"vin\">");
  char *vin = extract_feature(strstr(vin_section, "terminal\'>"), "</span>", 10, 30);

  char *engine_section = strstr(search_from, "<span class=\"key\" data-key=\"engine_number\">");
  char *engine = extract_feature(strstr(engine_section, "terminal\'>"), "<", 10, 20);

  char *transmission_section = strstr(search_from, "<span class=\"key\" data-key=\"transmission\">");
  char *transmission = extract_feature(strstr(transmission_section, "value\">"), "<", 7, 30);

  char *all_the_data[] = {vin, engine, transmission};

  int size = 4; 

  char **all_data_returnable = malloc(size * sizeof(char *));
  if (!all_data_returnable){
    printf("No good. all data returnable alloc failed");
    return NULL;
  }

  for (int feature = 0; feature < 3; feature++){
    all_data_returnable[feature] = all_the_data[feature];
  }
  all_data_returnable[3] = NULL;

  return all_data_returnable;

}

char **window_jph_there(data_holder *data){

  //find beginning of part we want to look through
  char *search_from = strstr(data->data, "<title>Report - ");

  char *vin = extract_feature(strstr(search_from, "vin\":\""), "\",", 6, 30);

  char *grade = extract_feature(strstr(search_from, "grade\":"), "\",", 8, 20);

  char *manufacture_date = extract_feature(strstr(search_from, "manufacture_date\":"), "\",", 19, 10);

  char *engine = extract_feature(strstr(search_from, "engine\":"), "\",", 9, 8);

  char *drive = extract_feature(strstr(search_from, "drive\":"), "\",", 8, 10);

  char *transmission = extract_feature(strstr(search_from, "transmission\":"), "\",", 15, 5);

  
  char * all_the_data[] = {grade, manufacture_date, engine, drive, transmission, vin};

  int size = 7; //max is 6 + null terminator
  
  char ** all_data_returnable = malloc(size * sizeof(char *));
  if (!all_data_returnable){ 
    printf("No good. all data returnable alloc failed");
    return NULL;
  }

  for (int feature = 0; feature < 6; feature++){
    all_data_returnable[feature] = all_the_data[feature];
  }
  all_data_returnable[6] = NULL;

  return all_data_returnable;
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
