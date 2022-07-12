# ⚡️Donation Alerts API

<p align="center">
<img src="https://i.imgur.com/l9OoaYG.png"></a>
</p>

С помощью этого плагина можно получать приходящие донаты на Donation Alerts в Unreal Engine.


## ✔️ _Как использовать_

1. Для работы плагина нужно зарегестрировать приложение на OAuth-сервере DonationAlerts.
https://www.donationalerts.com/application/clients
> _Дефолтный URL перенаправления для работы с этим плагином: http://localhost:8000/callback_

2. Добавьте компонент `CDonationAlertsIntegrator` к любому актору и используйте следующие функции:

- Connect - Открывает сайт в браузере для OAuth авторизации, а после происходит подключение к веб-сокету. 
> _При вызове функции нужно указать AppID(ID приложения) от вашего зарегестрированного приложения на [сайте DonationAlerts](https://www.donationalerts.com/application/clients). Остальные значения зависят от указанного URL перенаправления._

- Disconnect - Отключает текущие подключение к веб-сокету. К примеру это нужно на тот случай если пользователь захочет сменить аккаунт Donation Alerts.
- Делегат OnDonateReceived - Вызывается при получении доната.

<p align="center">
<img src="https://i.imgur.com/NowPA5D.png"></a>
</p>

## Дополнительно

[Официальная документация Donation Alerts API](https://www.donationalerts.com/apidoc)
<br>[Плагин для работы с Donation Alerts API от другого разработчика(Ufna)](https://github.com/ufna/DonationAlerts)

> _Различия: В плагине от [Ufna](https://github.com/ufna/) больше функционала и авторизация происходит в виджет браузере через модуль "WebBrowser", в моём плагине авторизация происходит в браузере пользователя(Chrome, Mozilla и.т.д.)._
