//plants.c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

//BASE WEBSITE = http://jmprog.com/gcmwebapp/deviceinput.php
char *BASE_URL = "http://jmprog.com/gcmwebapp/deviceinput.php";

struct url_data{
	size_t size;
	char* data;
};
//making an edit to main
size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data){

	size_t index = data->size;
	size_t n = (size*nmemb);
	char* tmp;

	data->size += (size * nmemb);

	fprintf(stderr, "data at %p size=%ld nmemb=%ld\n", ptr, size, nmemb);

	tmp = realloc(data->data, data->size + 1);

	if(tmp){

		data->data = tmp;

	}
	else{

		if(data->data){

			free(data->data);

		}
		fprintf(stderr, "FAILED\n");
		return 0;

	}

	memcpy((data->data + index), ptr, n);
	data->data[data->size] = '\0';
	return size*nmemb;

}

int sendNotification(char *name, char *message){

	//sets up the notification variables
	CURL *curl;
	CURLcode res;
	char UIDMessage[150];

	//The server requires the format of "intent=message&from_id=(phone name)&message=(intended notification)"
	strcpy(UIDMessage, "intent=message&from_id=");
	strcat(UIDMessage, name);
	strcat(UIDMessage, "&message=");
	strcat(UIDMessage, message);

	//initializes curl system (tells the pi how to send notifications)
	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();

	//if initialization went well...
	if(curl){

		//sends the variable UIDMessage to justin's server
		curl_easy_setopt(curl, CURLOPT_URL, BASE_URL);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, UIDMessage);

		res = curl_easy_perform(curl);

		//if internet isnt available; print error message
		if(res != CURLE_OK){

			printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			return 0;
		}

		curl_easy_cleanup(curl);
	}

	curl_global_cleanup();
	return 1;

}

char *keepAlive(char *name, int request){
	CURL *curl;
	CURLcode res;

	struct url_data data;
	data.size = 0;
	data.data = malloc(4096);
	if(NULL == data.data){

		fprintf(stderr, "FAIL\n");
		return NULL;

	}

	data.data[0] = '\0';

	char UIDMessage[150];
	if(request == 0){
		strcpy(UIDMessage, "intent=update_nj&from_id=");
		strcat(UIDMessage, name);
	}
	if(request == 1){

		strcpy(UIDMessage, "intent=ok_request2&from_id=");
		strcat(UIDMessage, name);
	}
	if(request == 2){

		strcpy(UIDMessage, "intent=ok_delete&from_id=");
		strcat(UIDMessage, name);
		strcat(UIDMessage, "&status=1");

	}
	if(request == 3){

		strcpy(UIDMessage, "intent=ok_delete&from_id=");
		strcat(UIDMessage, name);
		strcat(UIDMessage, "&status=2");
	}
	if(request == 4){

		strcpy(UIDMessage, "intent=ok_sensitivity&from_id=");
		strcat(UIDMessage, name);
		strcat(UIDMessage, "&status=1");

	}

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();

	if(curl){

		curl_easy_setopt(curl, CURLOPT_URL, "http://jmprog.com/gcmwebapp/deviceinput.php");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, UIDMessage);


		res = curl_easy_perform(curl);
		printf("Success?\n");

		if(res != CURLE_OK){

			printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			return 0;
		}


		curl_easy_cleanup(curl);
	}

	curl_global_cleanup();
	return data.data;


}

char *ackRequest(char *name1){

	CURL *curl1;
	CURLcode res1;

	struct url_data data;
	data.size = 0;
	data.data = malloc(4096);

	if(NULL == data.data){
		fprintf(stderr, "NO>>>\n");
		return NULL;
	}

	data.data[0] = '\0';

	char UIDMessage1[150];
	strcpy(UIDMessage1, "intent=get_request&from_id=");
	strcat(UIDMessage1, name1);

	curl_global_init(CURL_GLOBAL_ALL);

	curl1 = curl_easy_init();

	if(curl1){


		curl_easy_setopt(curl1, CURLOPT_URL, "http://jmprog.com/gcmwebapp/deviceinput.php");
		curl_easy_setopt(curl1, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl1, CURLOPT_WRITEDATA, &data);
		curl_easy_setopt(curl1, CURLOPT_POSTFIELDS, UIDMessage1);

		res1 = curl_easy_perform(curl1);

		if(res1 != CURLE_OK){

			printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res1));
			return 0;
		}

		curl_easy_cleanup(curl1);

	}

	curl_global_cleanup();

	return data.data;

}

int getUpdate(char *name){

	char *updateValue;
	updateValue = keepAlive(name, 0);
	if(updateValue[0] == '1'){
		updateValue = ackRequest(name);

		if(updateValue[0] == '3'){
			keepAlive(name, 4);
		}
		else{
			sendNotification(name, "Feature not supported in current build.");
			return 0;
		}

	}
	else{
		return 0;
	}
	return (int)(updateValue[2] - 48);

}
