#include "httpd.h"
#include "internet.h"

char *wan_if_name;
char *external_detwan_path;
char *host_name;
struct in6_addr from_ip6;
struct in_addr from_ip;

int main(int argc, const char **argv)
{
	char **argp = &argv[1];
	uint8 *ptrmac, dutMac[6], pcMac[6];
	int ConnType,addr_ok = -1,ret= -1;

	wan_if_name = NULL;
	external_detwan_path = NULL;

	while (argp < &argv[argc]) {
		if (strcasecmp(*argp, "-i") == 0)
			wan_if_name = *++argp;
		else if (strcasecmp(*argp, "-p") == 0){
			if(strstr(*++argp,":")){
				if((ret = inet_pton(AF_INET6,*argp,&from_ip6)) == 1)
					addr_ok = 6;
			}
			else{
				from_ip.s_addr = inet_addr(*argp);
				if(from_ip.s_addr != INADDR_NONE && from_ip.s_addr != INADDR_ANY)
					addr_ok = 4;
			}
		}
		else if (strcasecmp(*argp, "-d") == 0) {
			if ((ptrmac = (uint8 *) ether_aton(*++argp)) != NULL)
				memcpy(dutMac, ptrmac, 6);
			else
				memset(dutMac, 0x0, 6);
			syslog(6, "[DETWAN] *External* DutMAC: %s", *argp);
		} else if (strcasecmp(*argp, "-n") == 0) {
			if ((ptrmac = (uint8 *) ether_aton(*++argp)) != NULL)
				memcpy(pcMac, ptrmac, 6);
			else
				memset(pcMac, 0x0, 6);
			syslog(6, "[DETWAN] *External* PCMAC: %s", *argp);
		}

		argp++;
	}

	if (wan_if_name == NULL || addr_ok < 0) {
		printf
		    ("Usage: detwan -i (WAN interface name) -p (Managing PC's IP) -d (DutMAC) -n (PCMAC)\n");
		exit(-1);
	}

	host_name = (char *)malloc(sizeof(char) * 32);
	snprintf(host_name, sizeof(char) * 32, "%s",
		 config_get("wan_hostname"));

	ConnType = do_detection(dutMac, pcMac);

	syslog(6, "[DETWAN] *External* Result of do_detection: %d", ConnType);

	return ConnType;
}

