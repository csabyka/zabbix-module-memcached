/*
** Zabbix
** Copyright (C) 2001-2014 Zabbix SIA
**
** This program is free software; you can memcachedtribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** Author: Cao Qingshan <caoqingshan@kingsoft.com>
**
** Version: 1.1 Last update: 2015-12-03 20:00
**
**/

#include "common.h"
#include "sysinc.h"
#include "module.h"
#include "cfg.h"
#include "log.h"
#include "zbxjson.h"
#include "comms.h"

/* the variable keeps timeout setting for item processing */
static int	item_timeout = 0;

/* the path of config file */
const char	ZBX_MODULE_MEMCACHED_CONFIG_FILE[] = "/etc/zabbix/zbx_module_memcached.conf";

char		*CONFIG_MEMCACHED_INSTANCE_PORT = NULL;

static char	*MEMCACHED_DEFAULT_INSTANCE_HOST = "127.0.0.1";

#define ZBX_CFG_LTRIM_CHARS     "\t "
#define ZBX_CFG_RTRIM_CHARS     ZBX_CFG_LTRIM_CHARS "\r\n"

int	zbx_module_memcached_load_config(int requirement);
int	zbx_module_memcached_discovery(AGENT_REQUEST *request, AGENT_RESULT *result);
int	zbx_module_memcached_status(AGENT_REQUEST *request, AGENT_RESULT *result);
int	zbx_module_memcached_ping(AGENT_REQUEST *request, AGENT_RESULT *result);

static ZBX_METRIC keys[] =
/*      KEY                     FLAG		        FUNCTION        	          TEST PARAMETERS */
{
	{"memcached.discovery",	0,			zbx_module_memcached_discovery,	  NULL},
	{"memcached.status",	CF_HAVEPARAMS,		zbx_module_memcached_status,	  NULL},
	{"memcached.ping",	CF_HAVEPARAMS,		zbx_module_memcached_ping,	  NULL},
	{NULL}
};

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_api_version                                           *
 *                                                                            *
 * Purpose: returns version number of the module interface                    *
 *                                                                            *
 * Return value: ZBX_MODULE_API_VERSION_ONE - the only version supported by   *
 *               Zabbix currently                                             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_api_version()
{
	return ZBX_MODULE_API_VERSION_ONE;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_timeout                                          *
 *                                                                            *
 * Purpose: set timeout value for processing of items                         *
 *                                                                            *
 * Parameters: timeout - timeout in seconds, 0 - no timeout set               *
 *                                                                            *
 ******************************************************************************/
void	zbx_module_item_timeout(int timeout)
{
	item_timeout = timeout;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_list                                             *
 *                                                                            *
 * Purpose: returns list of item keys supported by the module                 *
 *                                                                            *
 * Return value: list of item keys                                            *
 *                                                                            *
 ******************************************************************************/
ZBX_METRIC	*zbx_module_item_list()
{
	return keys;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_memcached_load_config                                 *
 *                                                                            *
 * Purpose: load configuration from config file                               *
 *                                                                            *
 * Parameters: requirement - produce error if config file missing or not      *
 *                                                                            *
 ******************************************************************************/
int zbx_module_memcached_load_config(int requirement)
{
	int     ret = SYSINFO_RET_FAIL;

	struct cfg_line cfg[] =
	{
		/* PARAMETER,               VAR,                                      TYPE,
			MANDATORY,  MIN,        MAX */
		{"memcached_inst_ports",    &CONFIG_MEMCACHED_INSTANCE_PORT,          TYPE_STRING_LIST,
			PARM_OPT,   0,          0},
		{NULL}
	};

	parse_cfg_file(ZBX_MODULE_MEMCACHED_CONFIG_FILE, cfg, requirement, ZBX_CFG_STRICT);

	if (ZBX_CFG_FILE_REQUIRED == requirement && NULL == CONFIG_MEMCACHED_INSTANCE_PORT)
	{
		zabbix_log(LOG_LEVEL_WARNING, "Parameter memcached_inst_ports must be defined, example: 11211,11212");
		return ret;
	}
	else
	{
		ret = SYSINFO_RET_OK;
	}

	return ret;
}

int zbx_module_memcached_discovery(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char    		*p;
	struct zbx_json		j;
	char			*config = NULL;
	char			*f = NULL;
	char			*s = NULL; 

	zbx_json_init(&j, ZBX_JSON_STAT_BUF_LEN);

	zbx_json_addarray(&j, ZBX_PROTO_TAG_DATA);
	
	/* for dev
	zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_discovery], CONFIG_MEMCACHED_INSTANCE_PORT value is: [%s]", CONFIG_MEMCACHED_INSTANCE_PORT);
	*/

	if (NULL != CONFIG_MEMCACHED_INSTANCE_PORT && '\0' != *CONFIG_MEMCACHED_INSTANCE_PORT)
	{
		config = zbx_strdup(NULL, CONFIG_MEMCACHED_INSTANCE_PORT);

		p = strtok(config, ",");

		while (NULL != p)
		{
			f = p;
			if (NULL == (s = strchr(p, ':')))
			{
				zbx_lrtrim(f, ZBX_CFG_RTRIM_CHARS);

				zbx_json_addobject(&j, NULL);
				zbx_json_addstring(&j, "{#MCHOST}", MEMCACHED_DEFAULT_INSTANCE_HOST, ZBX_JSON_TYPE_STRING);
				zbx_json_addstring(&j, "{#MCPORT}", f, ZBX_JSON_TYPE_STRING);
				zbx_json_close(&j);
				/* for dev
				zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_discovery], add instance:[%s:%s] to discovery list", MEMCACHED_DEFAULT_INSTANCE_HOST, f);
				*/
			}
			else
			{
				*s++ = '\0';
				zbx_lrtrim(f, ZBX_CFG_RTRIM_CHARS);
				zbx_lrtrim(s, ZBX_CFG_RTRIM_CHARS);

				if (NULL != s && '\0' != s)
				{
					zbx_json_addobject(&j, NULL);
					zbx_json_addstring(&j, "{#MCHOST}", f, ZBX_JSON_TYPE_STRING);
					zbx_json_addstring(&j, "{#MCPORT}", s, ZBX_JSON_TYPE_STRING);
					zbx_json_close(&j);
					/* for dev
					zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_discovery], add instance:[%s:%s] to discovery list", f, s);
					*/
				}
				else
				{
					zbx_json_addobject(&j, NULL);
					zbx_json_addstring(&j, "{#MCHOST}", MEMCACHED_DEFAULT_INSTANCE_HOST, ZBX_JSON_TYPE_STRING);
					zbx_json_addstring(&j, "{#MCPORT}", f, ZBX_JSON_TYPE_STRING);
					zbx_json_close(&j);
					/* for dev
					zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_discovery], add instance:[%s:%s] to discovery list", MEMCACHED_DEFAULT_INSTANCE_HOST, f);
					*/
				}
			}
			p = strtok (NULL, ",");
		}
	}

	zbx_json_close(&j);

	SET_STR_RESULT(result, strdup(j.buffer));

	zbx_json_free(&j);

	return SYSINFO_RET_OK;

}

int	zbx_module_memcached_status(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char		*CONFIG_SOURCE_IP = NULL;	
	zbx_sock_t	s;
	char		*mc_host, *str_mc_port, *key;
	unsigned int    mc_port;
	char		mc_st_name[MAX_STRING_LEN];
	zbx_uint64_t 	mc_st_value;
	const char      *buf;
	char		*tmp;
	char		*p;
	int		ret = SYSINFO_RET_FAIL;
	int		find = 0;
	int		net_error = 0;

	if (request->nparam == 3)
	{
		mc_host = get_rparam(request, 0);
		str_mc_port = get_rparam(request, 1);
		mc_port = atoi(str_mc_port);
		key = get_rparam(request, 2);
	}
	else if (request->nparam == 2)
	{
		mc_host = MEMCACHED_DEFAULT_INSTANCE_HOST;
		str_mc_port = get_rparam(request, 0);
		mc_port = atoi(str_mc_port);
		key = get_rparam(request, 1);
	}
	else
	{
		/* set optional error message */
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid number of parameters"));
		return ret;
	}

	/* for dev
	zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_status], args:[%s,%d]", mc_host, mc_port);
	*/

	if (SUCCEED == zbx_tcp_connect(&s, CONFIG_SOURCE_IP, mc_host, mc_port, 0))
	{
		/* for dev
		zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_status], connect to [%s:%d] successful", mc_host, mc_port);
		*/

		if (SUCCEED == zbx_tcp_send_raw(&s, "stats\r\nquit\r\n"))
		{
			/* for dev
			zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_status], send request successful");
			*/

			while (NULL != (buf = zbx_tcp_recv_line(&s)) && 0 != strcmp(buf, "END"))
			{
				/* for dev
				zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_status], got [%s]", buf);
				*/
				if (2 == sscanf(buf, "%*s" "%s" ZBX_FS_UI64, mc_st_name, &mc_st_value))
				{
					/* for dev
					zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_status], parse line name:[%s] value:[%d]", mc_st_name, mc_st_value);
					*/
					if (0 == strcmp(mc_st_name, key))
					{
						/* for dev
						zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_status], args:[%d,%s] value:[%d]", mc_port, key, mc_st_value);
						*/
						find = 1;
						SET_UI64_RESULT(result, mc_st_value);
						ret = SYSINFO_RET_OK;
						break;
					}
				}
			}
		}
		else
		{
			net_error = 1;
			zabbix_log(LOG_LEVEL_WARNING, "module [memcached], func [zbx_module_memcached_status],get memcached status error: [%s]", zbx_tcp_strerror());
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Get memcached status error [%s]", zbx_tcp_strerror()));
		}

		zbx_tcp_close(&s);
	}
	else
	{
		net_error = 1;
		zabbix_log(LOG_LEVEL_WARNING, "module [memcached], func [zbx_module_memcached_status], get memcached status error: [%s]", zbx_tcp_strerror());
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Get memcached status error [%s]", zbx_tcp_strerror()));
	}

	if (find != 1 && net_error == 0)
	{
		zabbix_log(LOG_LEVEL_WARNING, "module [memcached], func [zbx_module_memcached_status], can't find key: [%s]", key);
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Not supported key [%s]", key));
	}

	return ret;
}

int	zbx_module_memcached_ping(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char            *CONFIG_SOURCE_IP = NULL;       
	zbx_sock_t      s;
	char            *mc_host, *str_mc_port;
	unsigned int    mc_port;
	const char      *buf;
	char            rv[MAX_STRING_LEN];
	int             mc_status = 0;
	time_t          now;
	char            str_time[MAX_STRING_LEN];
	char            cmd[MAX_STRING_LEN];
	char            hv[MAX_STRING_LEN];
	struct tm       *tm = NULL;


	if (request->nparam == 2)
	{
		mc_host = get_rparam(request, 0);
		str_mc_port = get_rparam(request, 1);
		mc_port = atoi(str_mc_port);
	}
	else if (request->nparam == 1)
	{
		mc_host = MEMCACHED_DEFAULT_INSTANCE_HOST;
		str_mc_port = get_rparam(request, 0);
		mc_port = atoi(str_mc_port);
	}
	else
	{
		/* set optional error message */
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid number of parameters"));
		return SYSINFO_RET_FAIL;
	}

	/* for dev
	zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_ping], args:[%s,%d]", mc_host, mc_port);
	*/

	time(&now);
	tm = localtime(&now);
	strftime(str_time, MAX_STRING_LEN, "%Y%m%d%H%M%S", tm);

	zbx_snprintf(cmd, MAX_STRING_LEN, "set ZBX_PING 521 60 %d\r\n%s\r\nget ZBX_PING\r\nquit\r\n",strlen(str_time), str_time);
	zbx_snprintf(hv, MAX_STRING_LEN, "STOREDVALUE ZBX_PING 521 %d%sEND", strlen(str_time), str_time);

	if (SUCCEED == zbx_tcp_connect(&s, CONFIG_SOURCE_IP, mc_host, mc_port, 0))
	{
		/* for dev
		zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_ping], connect to [%s:%d] successful", mc_host, mc_port);
		*/

		if (SUCCEED == zbx_tcp_send_raw(&s, cmd))
		{
			/* for dev
			zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_ping], send request successful");
			*/

			strscpy(rv, "");

			while (NULL != (buf = zbx_tcp_recv_line(&s)))
			{
				/* for dev
				zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_ping], get [%s]", buf);
				*/
				zbx_strlcat(rv, buf, MAX_STRING_LEN);
			}
			/* for dev
			zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_ping], all get [%s]", rv);
			*/
			if (0 == strcmp(rv, hv))
			{
				/* for dev
				zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_memcached_ping], memcached instance:[%s:%d] is up", mc_host, mc_port);
				*/
				mc_status = 1;
			}
		}

		zbx_tcp_close(&s);
	}

	if (mc_status == 1)
	{
		SET_UI64_RESULT(result, 1);
	}
	else
	{
		SET_UI64_RESULT(result, 0);
	}

	return SYSINFO_RET_OK;
}


/******************************************************************************
 *                                                                            *
 * Function: zbx_module_init                                                  *
 *                                                                            *
 * Purpose: the function is called on agent startup                           *
 *          It should be used to call any initialization routines             *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - module initialization failed               *
 *                                                                            *
 * Comment: the module won't be loaded in case of ZBX_MODULE_FAIL             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_init()
{
	zabbix_log(LOG_LEVEL_INFORMATION, "module [memcached], func [zbx_module_init], using configuration file: [%s]", ZBX_MODULE_MEMCACHED_CONFIG_FILE);

	if (SYSINFO_RET_OK != zbx_module_memcached_load_config(ZBX_CFG_FILE_REQUIRED))
		return ZBX_MODULE_FAIL;

	return ZBX_MODULE_OK;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_uninit                                                *
 *                                                                            *
 * Purpose: the function is called on agent shutdown                          *
 *          It should be used to cleanup used resources if there are any      *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - function failed                            *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_uninit()
{
	return ZBX_MODULE_OK;
}
