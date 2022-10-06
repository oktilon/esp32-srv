/** 
 * index.html
 */
#ifndef _INDEX_H
#define _INDEX_H 1

#define INDEX_HTML = "<!DOCTYPE html><html>\n    <head>\n        <title>ESP32 Сервак</title>\n        <style>\n            body {\n                font-size: 3rem;\n                text-align: center;\n                width: 100%%;\n            }\n            .unk {\n                color: gray;\n                font-weight: normal;\n            }\n            .on {\n                color: blue;\n                font-weight: bold;\n            }\n            .off {\n                color: lightcoral;\n                font-weight: bold;\n            }\n            .hdr {\n                font-weight: bold;\n                text-align: center;\n            }\n            .btn {\n                width: 30%%;\n                /* font-size: 4rem; */\n            }\n            .btnw {\n                width: 60%%;\n                /* font-size: 4rem; */\n            }\n        </style>\n    </head>\n    <body>\n        <div class=\"hdr\">ESP32 Сервак</div>\n        <div>LED is <span id='led' class=\"%s\">%s</span></div>\n        <div>\n            <button class=\"btn\" onclick=\"led_on()\">ON</button>\n            <button class=\"btn\" onclick=\"led_off()\">OFF</button>\n        </div>\n        <div>\n            <button class=\"btnw\" onclick=\"send()\">SEND</button>\n        </div>\n    </body>\n    <script type=\"application/javascript\">\n        function led_answer(e) {\n            if (this.readyState === 4) {\n                if (this.status === 200) {\n                    let led = document.getElementById(\"led\");\n                    if(this.responseText == 1) {\n                        led.className = \"on\";\n                        led.innerText = \"ON\";\n                    } else if(this.responseText == 0) {\n                        led.className = \"off\";\n                        led.innerText = \"OFF\";\n                    } else {\n                        led.className = \"unk\";\n                        led.innerText = \"?\";\n                    }\n                    return;\n                } else {\n                    alert(\"Status : \" + this.statusText);\n                    return;\n                }\n            }\n            alert(\"Not ready\");\n        }\n        function led_on() {\n            const req = new XMLHttpRequest();\n            req.open(\"GET\", \"/led_on\", true);\n            req.onload = led_answer;\n            req.onerror = (e) => { alert(\"Error : \" + req.statusText); };\n            req.send();\n        }\n        function led_off() {\n            const req = new XMLHttpRequest();\n            req.open(\"GET\", \"/led_off\", true);\n            req.onload = led_answer;\n            req.onerror = (e) => { alert(\"Error : \" + req.statusText); };\n            req.send();\n        }\n        function send() {\n            const req = new XMLHttpRequest();\n            req.open(\"GET\", \"/send\");\n            req.onload = (e) => {\n                if (req.readyState === 4) {\n                    if (req.status === 200) {\n                        alert(\"Answer : \" + this.responseText);\n                        return;\n                    } else {\n                        alert(\"Status : \" + this.statusText);\n                        return;\n                    }\n                }\n                alert(\"Not ready\");\n            }\n            req.onerror = (e) => { alert(\"Error : \" + req.statusText); };\n            req.send();\n        }\n    </script>\n</html>"

#endif // _INDEX_H
