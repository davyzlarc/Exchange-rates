#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <curl/curl.h>
#include <ctime>
#include <unistd.h>
#include "PugiXML/pugiconfig.hpp"
#include "PugiXML/pugixml.hpp"
#include "PugiXML/pugixml.cpp"

using namespace std;
using namespace pugi;

struct Values //Структура валюты
{
    string Date,  //Дата
        Currency; //Название валюты
    double Value; //Значение
};

class Quotes //Класс по работе с данными валюты, и хранению max и min значений
{
public:
    //Отрабатывает принимаемое значение валюты
    void SetValue(string Date, string Currency, double Value)
    {
        if (find_max(Value))
        {
            max_value.Date = Date;
            max_value.Currency = Currency;
            max_value.Value = Value;
        }
        if (find_min(Value))
        {
            min_value.Date = Date;
            min_value.Currency = Currency;
            min_value.Value = Value;
        }
        average(Currency, Value);
        //cout << Date << "  " << Currency << "  " << Value << endl;
    }

    //Вывод максимального значения валюты
    void Get_Max()
    {
        cout << "Maximum exchange rate: Currency - \"" << max_value.Currency << "\", Date - \"" << max_value.Date << "\", Value - \""
             << max_value.Value << "\"" << endl;
    }

    //Вывод минимального значения валюты
    void Get_Min()
    {
        cout << "Minimum exchange rate: Currency - \"" << min_value.Currency << "\", Date - \"" + min_value.Date << "\", Value - \""
             << min_value.Value << "\"" << endl;
    }

    //Вывод среднего значения рубля
    void Get_Average()
    {
        cout << "Average value of the ruble exchange rate for the entire period for:" << endl;
        for (const auto &[key, value] : rub_average)
            cout << setw(25)<<left << "\"" + key+  "\""<<" - " <<setw(12)<< right<<value.at(0) / value.at(1)  << endl; //Среднее через сумму и общее количество
    }
private:
    Values max_value = {"", "", 0}, min_value = {"", "", 0}; //Данные максимальной и минимальной валюты
    map<string, vector<double>> rub_average;                 //Среднее значения рубля
    bool default_min = true;                                 //Флаг изменения минимальной валюты

    //Проверка на максимальную валюту
    bool find_max(double Value)
    {
        if (Value > max_value.Value)
            return true;
        else
            return false;
    }

    //Проверка на минимальную валюту
    bool find_min(double Value)
    {
        if (default_min || Value < min_value.Value)
        {
            if (default_min)
                default_min = false;
            return true;
        }
        else
            return false;
    }

    //Расчет суммы всех валют
    void average(string Currency, double Value)
    {
        if (rub_average.count(Currency) == 0)
        {
            rub_average[Currency].push_back(0);
            rub_average[Currency].push_back(0);
        }
        else
        {
            rub_average[Currency].at(0) += Value; //Сумма значений курса по валюте
            rub_average[Currency].at(1)++;        //Количество значений курсов по валюте
        }
    }
};

class Time //Класс для хранения и изменения даты
{
public:
    //Получение текущей (из класса) даты
    string Get_Date()
    {
        return Output_data();
    }

    //Изменение даты на один день назад или сохранение текущей (сегодняшней) даты
    void Change_Date(int num)
    {
        if (num == 0)       //Первый вызов изменения даты
            time(&rawtime); //Сохраняем текущую (сегоднюю) дату
        else
            rawtime -= 3600 * 24;
    }

private:
    time_t rawtime; //Дата в секундах

    //Перевод даты из секунд в формат DD/MM/YYYY
    string Output_data()
    {
        struct tm *timeinfo;
        timeinfo = gmtime(&rawtime);
        string day = to_string(timeinfo->tm_mday),
               month = to_string(timeinfo->tm_mon + 1),
               year = to_string(timeinfo->tm_year + 1900);
        if (stoi(day) < 10)
            day = "0" + day;
        if (stoi(month) < 10)
            month = "0" + month;

        return day + "/" + month + "/" + year;
    }
};

//Замена символа substr_start на substr_end в строке str
string Change_sym(const char_t *str, string sstr, string substr_start, string substr_end)
{
    string num;
    if (str != "")
        num = str;
    else if (sstr != "")
        num = sstr;
    int point = num.find(substr_start);
    while (point != string::npos)
    {
        num.erase(point, 1);
        num.insert(point, substr_end);
        point = num.find(substr_start);
    }
    return num;
}

static string buffer;                     //Буфер с xml ответом
static char errorBuffer[CURL_ERROR_SIZE]; //Ошибка при считывании в буфер

//Запись в буфер
static int writer(char *data, size_t size, size_t nmemb, string *buffer)
{
    int result = 0;
    if (buffer != NULL)
    {
        buffer->append(data, size * nmemb);
        result = size * nmemb;
    }
    return result;
}

//Ручной парсер
vector<string> Parse()
{
    vector<string> result;
    if (buffer.find("<Valute") == string::npos)
    {
        result.insert(result.begin(), "-1"); //Код ошибки
        return result;
    }
    else
        while (buffer.find("<Valute") != string::npos)
        {
            result.push_back(buffer.substr(buffer.find("<Name>") + 6, buffer.find("</Name>") - buffer.find("<Name>") - 6));          //Название валюты
            result.push_back(buffer.substr(buffer.find("<Value>") + 7, buffer.find("</Value>") - buffer.find("<Value>") - 7));       //Значение валюты
            result.push_back(buffer.substr(buffer.find("<Nominal>") + 9, buffer.find("</Nominal>") - buffer.find("<Nominal>") - 9)); //Значение номинала
            buffer.erase(buffer.find("<Valute"), buffer.find("</Valute>") + 9 - buffer.find("<Valute"));
        }
    result.insert(result.begin(), buffer.substr(60, 10)); //Дата

    return result;
}

int main()
{
    Quotes quotes;
    Time time;

    cout << fixed << setprecision(8);

    //Выбор данных для обработки - уникальных дней (без выходных), для которых есть котировки, или всех,где котировки выходных равны последнему рабочему дню
    bool collect_unique = false; //Флаг уникальных данных
    cout << "Should i use only unique days, when currency exchange rates are set. Because the stock market is closed on weekends and there are no tradings (y/n) - ";
    string ch;
    bool uncorrect = true;
    int first = 0;
    while (uncorrect)
    {
        if (first != 0)
            cout << "Incorrect input, retry - ";
        cin >> ch;
        if (ch == "y")
        {
            uncorrect = false;
            collect_unique = true;
        }
        else if (ch == "n")
            uncorrect = false;
        first++;
    }

    bool pugixml_parse = false; //Флаг pugixml парсинга
    cout << "Which method of parsing xml should i use: 1. Pugixml, 2. Manual parsing (faster). Answer (1/2) - ";
    uncorrect = true;
    first = 0;
    while (uncorrect)
    {
        if (first != 0)
            cout << "Incorrect input, retry - ";
        cin >> ch;
        if (ch == "1")
        {
            uncorrect = false;
            pugixml_parse = true;
        }
        else if (ch == "2")
            uncorrect = false;
        first++;
    }

    cout << endl;

    for (auto i = 0; i < 90; i++)
    {
        cout << "\r"
             << "Progress: " << i * 100 / 90 << "%" << flush;
        CURL *curl_handle = curl_easy_init(); //Инициализация curl
        CURLcode res;                         //Код ошибки libcurl
        if (curl_handle)
        {
            time.Change_Date(i);
            curl_easy_setopt(curl_handle, CURLOPT_URL, ("http://www.cbr.ru/scripts/XML_daily_eng.asp?date_req=" + time.Get_Date()).c_str()); //Установка URL
            curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errorBuffer);                                                                 //Установка переменной для записи ошибок в буфер

            struct curl_slist *host = NULL;
            host = curl_slist_append(host, "host.com:443:222.222.222.222");
            curl_easy_setopt(curl_handle, CURLOPT_RESOLVE, host);
            //curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
            //curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);

            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writer); //Установка функции записи в буфер
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &buffer);    //Установка буфера
            res = curl_easy_perform(curl_handle);                         //Выполнение запроса
            if (res == CURLE_OK)
            {
                if (pugixml_parse) //Парсинг буфера с xml с помощью pugixml
                {
                    xml_document xml;
                    xml_parse_result result = xml.load_string(buffer.c_str());
                    if (!result) //Неуспешная загрузка в парсер буфера
                    {
                        cout << "\r"
                             << "Could not recognize data in server response: " << result.description() << endl;
                        return -02002;
                    }

                    if (collect_unique) //Меняем текущую дату в случае сбора уникальных данных
                        while (Change_sym(xml.child("ValCurs").attribute("Date").value(), "", ".", "/") != time.Get_Date())
                            time.Change_Date(i);

                    xpath_query Valute_query("/ValCurs/Valute"),
                        Name_query("Name/text()"),
                        Value_query("Value/text()"),
                        Nominal_query("Nominal/text()");
                    xpath_node_set xpath_Valutes = xml.select_nodes(Valute_query);
                    for (const auto &xpath_Valute : xpath_Valutes)
                    {
                        xml_node Valute = xpath_Valute.node(),
                                 Name = Valute.select_node(Name_query).node(),
                                 Value = Valute.select_node(Value_query).node(),
                                 Nominal = Valute.select_node(Nominal_query).node();
                        quotes.SetValue(time.Get_Date(), Name.value(), stod(Change_sym(Value.value(), "", ",", ".")) / stod(Change_sym(Nominal.value(), "", ",", ".")));
                    }
                }
                else //Ручной парсинг
                {
                    vector<string> result = Parse();

                    if (result.at(0) == "-1") //Неуспешный парсинг буфера
                    {
                        cout << "\r"
                             << "Could not recognize data in server response" << endl;
                        return -02002;
                    }
                    else
                    {
                        if (collect_unique) //Меняем текущую дату в случае сбора уникальных данных
                            while (Change_sym("", result.at(0), ".", "/") != time.Get_Date())
                                time.Change_Date(i);

                        result.erase(result.begin());
                        string Currency, Value;
                        int i = 0;
                        for (const auto &item : result)
                        {
                            if ((i + 1) % 3 == 0)
                                quotes.SetValue(time.Get_Date(), Currency, stod(Change_sym("", Value, ",", ".")) / stod(Change_sym("", item, ",", ".")));
                            i++;
                            Currency = Value;
                            Value = item;
                        }
                    }
                }
            }
            else //Неуспешное выполнение запроса libcurl
            {
                cout << "\r"
                     << "Could not resolve: "
                     << "http://www.cbr.ru/scripts/XML_daily_eng.asp?date_req=" + time.Get_Date()
                     << " (Error code " << errorBuffer << ")" << endl;
                return -02001;
            }
        }
        buffer = "";                    //Обнуление буфера
        curl_easy_cleanup(curl_handle); //Обнуление curl-запроса
    }
    cout << "\r"
         << "Progress: " << 100 << "%" << flush;
    sleep(1);
    cout << "\r";
    quotes.Get_Max();
    quotes.Get_Min();
    quotes.Get_Average();

    return 0;
}