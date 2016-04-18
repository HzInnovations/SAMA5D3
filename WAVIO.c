#include <stdio.h> //Allows the input/output of files
#include <string.h> //String manipulation
#include <bcm2835.h> //Library for use of the GPIO pins (bcm2835 is the name of the pi CPU/GPU combo)
#include <stdlib.h> //Standard C library, without this, nothing would happen
#include <unistd.h> //Lets the program access low level OS system
#include <math.h> //Allows use of math functions, specifically pow()
#include <time.h> //Gives access to the timer, to time microphone sample rate
#include <fcntl.h> //Used for Serial
#include <termios.h> //Used for Serial
#include <sys/types.h> // Used for searching directories
#include <dirent.h> //Used to get access to directories
#include <stdbool.h>
#include <pthread.h>
#include <floatfann.h>
#include <fann.h>
#include <pthread.h>
#include <poll.h>
#include <curl/curl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include "kiss_fftr.h"
#include "plants.h"

//char *GUID = "Alpha719";
void sendMessageBetweenThreads(char *message);
struct btInfo{

	struct sockaddr_rc loc_addr;
	struct sockaddr_rc rem_addr;
	char buf[200];
	char response[200];
	int s, client;
	socklen_t opt;

};

struct ftpFile{
	const char *filename;
	FILE *stream;
};

static size_t writeFunction(void *buffer, size_t size, size_t nmemb, void *stream){

	struct ftpFile *out=(struct ftpFile *)stream;

	if(out && !out->stream){
		out->stream = fopen(out->filename, "wb");
		if(!out->stream){
			return -1;
		}
	}

	return fwrite(buffer, size, nmemb, out->stream);

}

void getSoftwareUpdate(struct fann *ann){

	CURL *curl;
	CURLcode res;
	struct ftpFile ftp={
		"train.wavio",
		NULL
	};

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if(curl){

		curl_easy_setopt(curl, CURLOPT_URL, "www.jmprog.com/AtmelUpdate/training.wavio");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ftp);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

		res=curl_easy_perform(curl);

		curl_easy_cleanup(curl);

		if(res != CURLE_OK){
			printf("UPDATE FAILED... %d\n", res);
		}

	}

	if(ftp.stream){
		int updater;

		fclose(ftp.stream);
		printf("LETS GOOOO\n");
		curl_global_cleanup();

		printf("LOCKED BY UPDATE\n");
		ann = fann_create_standard(4, 3780, 1900, 500, 2);

		fann_set_activation_function_hidden(ann, FANN_SIGMOID_SYMMETRIC);
		fann_set_activation_function_output(ann, FANN_SIGMOID_SYMMETRIC);

		fann_train_on_file(ann, "train.wavio", 1000, 1, .01);

		fann_save(ann, "wavio.net");
		printf("LOADING..");
		ann = fann_create_from_file("wavio.net");

	}

}

int softwareUpdate(struct fann *ann){

	CURL *curl;
	CURLcode res;
	struct ftpFile ftp={
		"updateAvailable.txt",
		NULL
	};

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if(curl){

		curl_easy_setopt(curl, CURLOPT_URL, "www.jmprog.com/AtmelUpdate/updateAvailable.txt");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ftp);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

		res = curl_easy_perform(curl);

		curl_easy_cleanup(curl);

		if(res != CURLE_OK){
 			printf("so... %d\n", res);
		}

	}

	if(ftp.stream){
		int updater;

		fclose(ftp.stream);
		printf("SAFE TO OPEN\n");
		curl_global_cleanup();

		FILE *softwareUpdate;
		softwareUpdate = fopen("updateAvailable.txt", "r");
		fscanf(softwareUpdate, "%d", &updater);

		printf("Software Update available? %d\n", updater);

		if(updater == 1){

			sendMessageBetweenThreads("Update Available\0");
			getSoftwareUpdate(ann);

		}
		else{
			sendMessageBetweenThreads("No Update Available\0");
		}
		return updater;
	}
	return -1;

}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct btInfo bluetoothPartner ={0};
struct btInfo emptyBTBOX = {0};
char sender[200] = {0};
char receiver[200] = {0};
char empty[200] = {0};
bool received = 0;

void closeBT(void *arg){

	pthread_mutex_lock(&mutex);
	close(bluetoothPartner.client);
	close(bluetoothPartner.s);
	pthread_mutex_unlock(&mutex);

}

void connectToBluetooth(void *arg){

	pthread_mutex_lock(&mutex);
//	printf("\n\nOwned by Connection Thread\n\n");
	bluetoothPartner = emptyBTBOX;

	bluetoothPartner.opt=sizeof(socklen_t);
	bluetoothPartner.s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	bluetoothPartner.loc_addr.rc_family = AF_BLUETOOTH;
	bluetoothPartner.loc_addr.rc_bdaddr = *BDADDR_ANY;
	bluetoothPartner.loc_addr.rc_channel = (uint8_t) 1;

	bind(bluetoothPartner.s, (struct sockaddr *)&bluetoothPartner.loc_addr, sizeof(bluetoothPartner.loc_addr));

	listen(bluetoothPartner.s, 1);

	bluetoothPartner.client = accept(bluetoothPartner.s, (struct sockaddr *)&bluetoothPartner.rem_addr, &bluetoothPartner.opt);

	ba2str(&bluetoothPartner.rem_addr.rc_bdaddr, bluetoothPartner.buf);
	printf("Accepted from %s\n", bluetoothPartner.buf);
	memset(bluetoothPartner.buf, 0, sizeof(bluetoothPartner.buf));

	connect(bluetoothPartner.s, (struct sockaddr *)&bluetoothPartner.rem_addr, sizeof(bluetoothPartner.rem_addr));
	write(bluetoothPartner.client, "Connected", 9);

	pthread_mutex_unlock(&mutex);

}
int sendMessage(char *message, int stringSize){

	int status;
	pthread_mutex_lock(&mutex);
//	printf("\n\nOwned by sending Thread\n\n");
	connect(bluetoothPartner.s, (struct sockaddr *)&bluetoothPartner.loc_addr, sizeof(bluetoothPartner.loc_addr));
	status = write(bluetoothPartner.client, message, stringSize);
	pthread_mutex_unlock(&mutex);
	if(status < 0){
		printf("Nothing is there.\n");
		closeBT(NULL);
		connectToBluetooth(NULL);
	}
	return status;

}

void *receiveThread(void *arg){

	struct pollfd fd;
	int ret;

	pthread_mutex_lock(&mutex);
	fd.fd = bluetoothPartner.client;
	fd.events = POLLIN;
	pthread_mutex_unlock(&mutex);

	while(1){
		pthread_mutex_lock(&mutex);
//		printf("\n\nOwned by Polling Thread\n\n");
		ret = poll(&fd,1,100);
		pthread_mutex_unlock(&mutex);

		if(ret == 0){
			printf("No Data Available from Poll.\n");
		}
		else if(ret == -1){
			printf("IDK MAN... ERROR\n");
		}
		else{
			printf("Data Available on socket: %d\n", ret);

			//Used to test the connection; poll can doop us sometimes.
			int wentWell;
			wentWell = sendMessage("Received", 8);

			if(wentWell >= 0){
				pthread_mutex_lock(&mutex);
				received = 1;

				int i;
				for(i = 0; i<200; i++){
					receiver[i] = 0;
				}

				read(bluetoothPartner.client, receiver, sizeof(receiver));
				if(receiver[0] == '-' && receiver[1] == '&'){
//					softwareUpdate(ann);
				}
				if(receiver[0] == '&' && receiver[1] == '-'){
					sendNotification("Papaya", "Current Version 0.3");
				}
				printf("%s\n", receiver);

				pthread_mutex_unlock(&mutex);
			}
		}

		bcm2835_delay(500);
	}

}

void *Bluetooth(void *btParam){

	connectToBluetooth(NULL);

	pthread_t receiveBT;

	if(pthread_create(&receiveBT, NULL, receiveThread, NULL)){

		printf("Cant make a receive thread\n");

	}
	pthread_detach(receiveBT);

//	sendMessage("Aaron Burr Sir", 14);

	while(1){

		pthread_mutex_lock(&mutex);
//		printf("\n\nOwned by multi-thread manager\n\n");
		bool emptySender = 1;
		int i;
		size_t size = 0;

		for(i = 0; i<200; i++){
			if(sender[i] != 0){
				emptySender = 0;
			}
		}

		if(emptySender == 0){

			for(i = 0; i<200; i++){
 				if(sender[i] == 0){
					size = i;
					break;
				}
			}
			printf("Message: %s\nSize of Message: %d\n", sender, size);

			pthread_mutex_unlock(&mutex);
			sendMessage(sender, size);
			pthread_mutex_lock(&mutex);

			for(i = 0; i<200; i++){
				sender[i] = 0;
			}
		}
		pthread_mutex_unlock(&mutex);
		bcm2835_delay(500);

	}

}

int connectInternet(char *ssid, char *pass){
        printf("Info Provided:\nSSID: %s\nPass: %s\n", ssid, pass);
        system("rm /etc/wpa_supplicant/wpa_supplicant.conf");
        FILE *network;

        network = fopen("/etc/wpa_supplicant/wpa_supplicant.conf", "w");
        fprintf(network, "country=GB\nctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\nupdate_config=1\nnetwork={\n\t");
        fprintf(network, "ssid=\"%s\"\n\tpsk=\"%s\"\n}", ssid, pass);

        fclose(network);
        bcm2835_delay(100);
	sendMessage("Wait for 'Loading AI' to disconnect from Bluetooth", 49);
	sendMessage("Attempting to Connect... Takes ~ 30 seconds",43);
	system("wpa_cli reconfigure");
	bcm2835_delay(10000);

	sendMessage("20 seconds left...", 18);

	bcm2835_delay(10000);

	sendMessage("10 seconds left...", 18);

	bcm2835_delay(10000);

        CURL *curl;
        CURLcode res;

        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            res = curl_easy_perform(curl);
            if(res != CURLE_OK){

                printf("Internet Connection not established\nTry Again: %s\n", curl_easy_strerror(res));
                pthread_mutex_lock(&mutex);
		strcpy(sender, "No Success\0");
		pthread_mutex_unlock(&mutex);
                return -1;

            }
            else{

                pthread_mutex_lock(&mutex);
		strcpy(sender, "Success\0");
		pthread_mutex_unlock(&mutex);

            }

            curl_easy_cleanup(curl);
        }
    printf("You have successfully connected to the internet.\n");
	curl_global_cleanup();
	return 1;

}


float * pascal(int sizeA){

    int centerFrequency, i;
    static float tri[100];

    if(sizeA % 2 == 0){
        centerFrequency = sizeA/2;
    }

    if(sizeA % 2 == 1){
        centerFrequency = (sizeA/2)+1;
    }

    tri[centerFrequency] = 1;
    for(i = 0; i<centerFrequency; i++){
        tri[i] = (1.0/(float)centerFrequency)*(i+1);
    }

    for(i = centerFrequency; i<sizeA; i++){
        tri[i] = tri[sizeA-i-1];
    }

    return tri;

}

void sendMessageBetweenThreads(char *message){

	int locked = -1;

	locked = pthread_mutex_trylock(&mutex);
	if(locked == 0){
		strcpy(sender, message);
		pthread_mutex_unlock(&mutex);
	}
	else{
		printf("Busy\n");
	}

}

int main(void){
	//system("killall wavio");
//	char *GUID = "Alpha719";
	//keepAlive(GUID);
//	sendNotification(GUID, "Wavio Online");
	//bcm2835_init();
	/*char internet[10] = {0};
        char ssid[200];
        char pass[200];
        int iSuccess;

	getConnected();
	bcm2835_delay(100);
	while(internet[0] != 'y' && internet[0] != 'n'){
		sendMessageWithResponse("Would you like to configure a new WiFi Network? (y/n)", 53, internet);

		if(internet[0] == 'y'){
			wifiConfigured = 1;
		}
		else if(internet[0] == 'n'){
			wifiConfigured = 0;
		}
		else{
			sendMessage("Invalid Response", 16);
		}

		if(wifiConfigured == 1){
        		do{
        	    		sendMessageWithResponse("SSID:", 5, ssid);
        	    		sendMessageWithResponse("Pass:", 5, pass);
        	    		iSuccess = connectInternet(ssid, pass);
        		}while(iSuccess == -1);
		}
	}

	sendMessage("Wavio Version 0.1",17);
	bcm2835_delay(100);*/

	char *GUID = "Papaya";
	printf("Wavio Version 0.3\n");

	bcm2835_init();

	pthread_t btThread;

	if(pthread_create(&btThread, NULL, Bluetooth, NULL)){
		printf("Yeah. No.\n");
	}
	pthread_detach(btThread);
	char check[200];
	int iSuccess = -1;
	bcm2835_delay(1000);
	sendMessage("Update WiFi Information? (y/n)",31);

	while(iSuccess == -1){

		pthread_mutex_lock(&mutex);
		if(received == 1){

			strcpy(check, receiver);
			received = 0;
			iSuccess = 0;
		}
		pthread_mutex_unlock(&mutex);
		bcm2835_delay(1000);

	}
	if(check[0] == 'y'){
		iSuccess = -1;
	}
	else{
		iSuccess = 0;
		sendMessageBetweenThreads("Skipping WiFi.\0");
	}
	while(iSuccess == -1){

		int filled = 0;
		bcm2835_delay(1000);
		pthread_mutex_lock(&mutex);
		strcpy(sender, "SSID:\0");
		pthread_mutex_unlock(&mutex);

		char ssid[200];
		char pass[200];
		int count = 0;

		while(filled == 0){
			pthread_mutex_lock(&mutex);
			if(received == 1){
				strcpy(ssid, receiver);

				int qw = 0;
				for(qw = 0; qw<25; qw++){
					printf("%c", receiver[qw]);
				}
				printf("\nLook Good: SSID?\n");

				filled = 1;
				received = 0;
				printf("SSID: %s\n", ssid);
				strcpy(sender, "Pass: \0");
			}
			else{
				printf("Nothing Yet...\n");
			}
			pthread_mutex_unlock(&mutex);
			bcm2835_delay(1500);
		}

		filled = 0;

		while(filled == 0){
			pthread_mutex_lock(&mutex);
			if(received == 1){
				strcpy(pass, receiver);
				int qw = 0;

				for(qw = 0; qw<25; qw++){
					printf("%c", receiver[qw]);
				}
				printf("\nLook Good?\n");

				filled = 1;
				received = 0;
				printf("PASS: %s\n", pass);
			}
			else{
				printf("Nothing Yet...\n");
			}
			pthread_mutex_unlock(&mutex);
			bcm2835_delay(1500);
		}

		printf("\n\nHERES YOUR INFO: %s;%s\n\n", ssid, pass);

		iSuccess = connectInternet(ssid, pass);
	}

//	softwareUpdate();

	bcm2835_spi_begin();
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST); //When requesting numbers, the MSB is received first
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); //spi mode 0 means that data is sent on the falling edge of the clock edge
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256); //SPI clock; 250MHz/256 = 976.56 kHz
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0); //Use the SPI device #0
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); //Slave select is active low

	char receiveBytes[3];
	char sendBytes[3];
	sendBytes[0] = 0x01;
	sendBytes[1] = 0x80;
	sendBytes[2] = 0x00;
	int soundData[1024];

	int k, i, j, msb, lsb;

	int N = 1024;

	double captureArray[N/2][30];

	kiss_fft_scalar in[N];
	kiss_fft_cpx out[N];
	kiss_fftr_cfg cfg;
	cfg = kiss_fftr_alloc(N, 0, NULL, NULL);

	int volumeThreshold = 15000;
	int reset = 0;

	i=0;
	j=0;
	k=0;

	fann_type *calc_out;
	fann_type input[3780];

	sendMessageBetweenThreads("Loading AI...\0");
	struct fann *ann = fann_create_from_file("wavio.net");

	sendMessageBetweenThreads("Finished\0");

//	sendMessage("Finished", 8);
/*	FILE *trainer;
		printf("Already Exists; Append\n");
		trainer = fopen("training.wavio", "a");
*/
float maxer[30]={0};
while(1){
	j = 0;
	while(j < 30){
		maxer[j] = 0;

		for(k = 0; k<N; k++){

			bcm2835_spi_transfernb(sendBytes, receiveBytes, 3);

			msb = (int)receiveBytes[1];
			lsb = (int)receiveBytes[2];

			msb = msb & 0x03;
			soundData[k] = (msb << 8) | lsb;

		}

		for(i=0; i<N; i++){
			soundData[i] = 0.5 * (1 - cos(2*3.14159265358979*i/1023)) * soundData[i];
			in[i] = soundData[i];
			if(in[i] > 1022){
				printf("Too Loud\n");
			}
		}

		kiss_fftr(cfg, in, out);

		for(i = 0; i<N/2; i++){

			captureArray[i][j] = sqrt(out[i].i*out[i].i+out[i].r*out[i].r);

			if(i == 0 || i == 1){
				captureArray[i][j] = 0;
			}
			if(captureArray[i][j] > maxer[j] && (i!=1 || i!=2)){
				maxer[j] = captureArray[i][j];
			}
		}
		printf("Volume: %d\n", maxer[j]);
		if(maxer[j] > volumeThreshold){
			j++;
			reset = 0;
		}
		else{
			reset++;
		}

		if(reset == 50){

			j = 0;
			reset = 0;
			int k;
//			keepAlive(GUID);
			printf("RESET\n");

		        sendMessageBetweenThreads("Too Quiet.\0");
		}

	}

	//keepAlive(GUID);
	for(i=0; i<30; i++){
		for(j = 2; j<128; j++){
			input[i*126 + (j-2)] = captureArray[j][i]/maxer[i];
		}
	}
	calc_out = fann_run(ann, input);

	printf("Test !\nDoorbell?: %f\nSmoke Alarm?: %f\n", calc_out[0], calc_out[1]);
	char final[50];
	if(calc_out[0] > 0.3 && calc_out[0] < 0.65){

		snprintf(final,50, "Maybe Doorbell?/Score: %f\0", calc_out[0]);

		sendMessageBetweenThreads(final);
	//	sendMessageBetweenThreads(out);
	}
	if(calc_out[0] >= 0.65){
		snprintf(final,50, "Doorbell/Score: %f\0", calc_out[0]);

		sendMessageBetweenThreads(final);
		sendNotification(GUID, "Doorbell");
	//	sendMessageBetweenThreads(out);
	}
	if(calc_out[0] <= 0.3){
		snprintf(final,50, "Not a doorbell/Score: %f\0", calc_out[0]);

		sendMessageBetweenThreads(final);
	//	sendMessageBetweenThreads(out);
	}

/*
	for(i = 0; i<30; i++){
		for(j = 2; j<128; j++){
			fprintf(trainer, "%f ", captureArray[j][i]/maxer[i]);
		}
	}
	fprintf(trainer, "\r\n");
	fprintf(trainer, "1 -1\r\n");
	iter[3]++;
	printf("%d\n", iter[3]);
*/

}
//fcloseall();
printf("Exited...\n");
}
