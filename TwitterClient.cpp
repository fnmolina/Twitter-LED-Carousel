#include "TwitterClient.h"

/*//////////////OBSERVACIONES/////////////
* Ver tipos y notificacion de errores
* Ver que se podria hacer dentro del while (stillRunning)
* Ver tema de curl y multiHandle como datos miembro
* Hace falta un try-catch al final del requestTweets??
* Ver tema de tweets parciales y cancelacion
*/


TwitterClient::TwitterClient(bool* cancelRequest, int* clientError)
{
	this->cancelRequest = cancelRequest;
	this->clientError = clientError;
	*this->clientError = CONNECTION_ERROR;
	this->curl = NULL;						//Guarda configuraciones de una transferencia.
	this->multiHandle = NULL;				//Administra easy handles.
	this->j.clear();						//json con informacion recibida de Twitter.
	this->token = "";						//Guarda token de acceso para recibir tweets.
	this->query = "";						// Direccion de Twitter que se va a consultar. 
	this->tweetsNum = "";					//Guarda string con la cantidad de tweets.
	this->stillRunning = 0;					//Para manejo de estado de procesamiento
	this->errorMessage = "";				//
	//this->m = NULL;						//Puntero a estructura de CURLmsg.
	//Datos de autentificacion de usuario de Twitter Developer. Por default, los datos provistos por la catedra.
	API_key = "HCB39Q15wIoH61KIkY5faRDf6";
	API_SecretKey = "7s8uvgQnJqjJDqA6JsLIFp90FcOaoR5Ic41LWyHOic0Ht3SRJ6";

	tweetsReady = false;
	this->readString = "";					//Para lectura de datos devueltos
	header = "";
}

//Posibilidad de cambio de usuario para acceder a los tuits.
void TwitterClient::setUserLoginData(std::string key, std::string SecretKey)
{
	std::string API_key = key;
	std::string API_SecretKey = SecretKey;
}

//Se introduce usuario y cantidad de tuits a recuperar.
void TwitterClient::setQuery(std::string twitterUser)
{
	this->query = "https://api.twitter.com/1.1/statuses/user_timeline.json?screen_name=" + twitterUser + "&count=";
}

//Devuelve indicador de finalizacion del proceso
bool TwitterClient::isReady()
{
	return tweetsReady;
}

//Devuelve tweets en formato json
json TwitterClient::getTweets()
{
	this->j = json::parse(readString);
	return this->j;
}

//Devuelve descripcion de error segun el tipo. 
std::string TwitterClient::getErrorMessage()
{
	return errorMessage;
}

//Configuracion de cURL, autentificacion con Outh2 y obtencion del token de Twitter.
int TwitterClient::requestBearerToken()
{
	std::string readString;			//Guarda todo lo que se recibe, usado como userData.
	json auxJson;

	this->curl = curl_easy_init();
	if (this->curl)
	{
		//Se setea la pagina donde nos vamos a conectar para buscar el token.
		curl_easy_setopt(this->curl, CURLOPT_URL, "https://api.twitter.com/oauth2/token");

		//Si la p�gina nos redirige a alg�n lado, le decimos a curl que siga dicha redirecci�n.
		curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, 1L);

		//Le decimos a CURL que trabaje tanto con HTTP como HTTPS. Autenticaci�n por HTTP en modo b�sico.
		curl_easy_setopt(this->curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
		curl_easy_setopt(this->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

		//Preparamos el password para la autenticaci�n
		std::string userPwd = this->API_key + ":" + this->API_SecretKey;
		//Se lo seteamos a CURL.
		curl_easy_setopt(this->curl, CURLOPT_USERPWD, userPwd.c_str());

		struct curl_slist* list = NULL;

		//Le decimos a CURL que vamos a mandar URLs codificadas y en formato UTF8.
		list = curl_slist_append(list, "Content-Type: application/x-www-form-urlencoded;charset=UTF-8");
		curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, list);

		//Le decimos a CURL que trabaje con credentials.
		std::string data = "grant_type=client_credentials";
		curl_easy_setopt(this->curl, CURLOPT_POSTFIELDSIZE, data.size());
		curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, data.c_str());

		/* Le decimos a curl que cuando haya que escribir llame a myCallback
		   y que use al string readString como user data. */
		curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, myCallback);
		curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &readString);

		// Perform the request, output will get the return code
		// Se intenta conectar a la pagina para obtener la autentificacion. 
		output = curl_easy_perform(this->curl);

		// De haber algun error, se indica en terminal.
		if (output != CURLE_OK)
		{
			std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(output) << std::endl;
			//Se liberan los recursos de curl antes de salir.
			curl_easy_cleanup(this->curl);
			//Se notifica error de conexion con Twitter.
			*this->clientError = CONNECTION_ERROR;
			return 0;
		}
		

		//Si no hubieron errores, se hace un clean up antes de realizar un nuevo query.
		curl_easy_cleanup(this->curl);

		/*	Si el request de CURL fue exitoso entonces twitter devuelve un JSON.
			Se busca dentro del json el token para acceso de los tuits. */
		auxJson = json::parse(readString);

		//Se encierra el parseo en un bloque try-catch porque la libreria maneja errores por excepciones.
		try
		{
			//Tratamos de acceder al campo acces_token del JSON
			std::string aux = auxJson["access_token"];
			this->token = aux;
			//std::cout << "Bearer Token get from Twitter API: \n" << this->token << std::endl;
		}
		catch (std::exception& e)
		{
			//Si hubo algun error, se muestra el error que devuelve la libreria
			std::cerr << e.what() << std::endl;
			return 0;
		}
	}
	else
	{
		std::cout << "Cannot download tweets. Unable to start cURL" << std::endl;
		return 0;
	}
	return 1;
}

void TwitterClient::configTweetsRequest(std::string count)
{
	this->tweetsNum = count;
	//Se agrega al query la cantidad de tuits a descargar.
	//std::string completeQuery = this->query + this->tweetsNum;

	//this->stillRunning = 0;			//Se inicializa proceso
	this->j.clear();				//Se limpian mensajes anteriores

	//Se inicializa nuevamente cURL, esta vez de forma simultanea y asincronica.
	this->curl = curl_easy_init();
	this->multiHandle = curl_multi_init();
	this->readString = "";	//Para lectura de datos devueltos
	this->tweetsReady = false;
	this->header = "";
	this->errorMessage = "";
}

//Solicitud de tuits de un usuario de preferencia.
int TwitterClient::requestTweets()
{
	this->tweetsReady = false;
	//int msgq = 0;
	//Por default, se setea el json de error como vacio.
	json errorJson;
	errorJson["myCount"] = nullptr;

	if ((curl != NULL) && (multiHandle != NULL))
	{
		//Attacheo el easy handle para manejar una coneccion no bloqueante.
		curl_multi_add_handle(multiHandle, curl);

		//Seteamos URL FOLLOWLOCATION y los protocolos a utilizar.
		curl_easy_setopt(curl, CURLOPT_URL, (this->query + this->tweetsNum).c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &this->header);

		//Construimos el Header de autenticacion como lo especifica la API usando el token obtenido.
		struct curl_slist* list = NULL;
		std::string aux = "Authorization: Bearer ";
		aux = aux + this->token;
		list = curl_slist_append(list, aux.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

		//Seteamos los callback.
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, myCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readString);

		//Se realiza ahora un perform no bloqueante.
		curl_multi_perform(multiHandle, &stillRunning);

		//Si se cancel� la request en algun momento, se para el proceso.
		if (*cancelRequest)
		{
			curl_multi_remove_handle(multiHandle, curl);
		}
		
		if (output != CURLE_OK)
		{
			std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(output) << std::endl;
			//Se liberan los recursos antes de salir.
			curl_easy_cleanup(curl);
			errorMessage = "cURL failed";
			this->j = errorJson;
			return 0;
		}


		//Si el request de CURL fue exitoso entonces twitter devuelve un JSON
		//con toda la informacion de los tweets que le pedimos
		if (stillRunning == 0)	//Si el proceso termino, se escribe el json
		{
			//Analiza header. De haber error, avisa al usuario.
			if (!headerParser())
			{
				this->j = errorJson;
				this->tweetsReady = false;
				curl_multi_cleanup(multiHandle);
				curl_easy_cleanup(curl);
				return 0;
			}
			else 
			{
				this->tweetsReady = true;
				//Cleanup al final.
				curl_multi_cleanup(multiHandle);
				curl_easy_cleanup(curl);
				return 1;
			}
		}
	}
	//En caso de no haber podido realizar correctamente la conexion, error. 
	else
	{
		std::cout << "Cannot download tweets. Unable to start cURL" << std::endl;
		//Se notifica error de conexion a usuario
		*this->clientError = NO_USER_ERROR;
		//Cleanup al final.
		curl_easy_cleanup(curl);
		this->j = errorJson;
		errorMessage = "Connection failed";
		return 0;
	}
	return 1;
}


bool TwitterClient::headerParser()
{
	bool check = false;
	//Caso entrega correcta del tuit.
	if (header.find("200 OK") != std::string::npos)
	{
		errorMessage = "200 OK";
		check = true;
		return check;
	}
	else if (header.find("304 Not Modified") != std::string::npos)
	{
		errorMessage = "304 Not Modified";
		return check;
	}
	else if (header.find("400 Bad Request") != std::string::npos)
	{
		errorMessage = "400 Bad Request";
		return check;
	}
	else if (header.find("401 Unauthorized") != std::string::npos)
	{
		errorMessage = "401 Unauthorized";
		return check;
	}
	else if (header.find("403 Forbidden") != std::string::npos)
	{
		errorMessage = "403 Forbidden";
		return check;
	}
	else if (header.find("404 Not Found") != std::string::npos)
	{
		errorMessage = "404 Not Found";
		return check;
	}
	else if (header.find("406 Not Acceptable") != std::string::npos)
	{
		errorMessage = "406 Not Acceptable";
		return check;
	}
	else if (header.find("410 Gone") != std::string::npos)
	{
		errorMessage = "410 Gone";
		return check;
	}
	else if (header.find("422 Unprocessable Entity") != std::string::npos)
	{
		errorMessage = "422 Unprocessable";
		return check;
	}
	else if (header.find("429 Too Many Requests") != std::string::npos)
	{
		errorMessage = "429 Too Many Req";
		return check;
	}
	else if (header.find("500 Internal Server Error") != std::string::npos)
	{
		errorMessage = "500 Server Error";
		return check;
	}
	else if (header.find("502 Bad Gateway") != std::string::npos)
	{
		errorMessage = "502 Bad Gateway";
		return check;
	}
	else if (header.find("502 Service Unavailable") != std::string::npos)
	{
		errorMessage = "502 Unavailable";
		return check;
	}
	else if (header.find("504 Gateway Timeout") != std::string::npos)
	{
		errorMessage = "504 Timeout";
		return check;
	}
	return check;
}






