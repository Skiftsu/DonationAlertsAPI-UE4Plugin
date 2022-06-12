# ⚡️Donation Alerts API

<p align="center">
<img src="https://i.imgur.com/l9OoaYG.png"></a>
</p>


## ✔️ _Как использовать_

1. Для работы плагина нужно зарегестрировать приложение на [сайте DonationAlerts](https://www.donationalerts.com/application/clients).
> URL перенаправления: http://localhost:8000/callback

2. Добавьте компонент `CDonationAlertsIntegrator` к актору и используйте следующие функции:

<p align="center">
<img src="https://i.imgur.com/NowPA5D.png"></a>
</p>

- Connect - Открывает сайт в браузере для авторизации, после происходит подключение к веб-сокету. 
> При вызове функции нужно указать AppID(ID приложения) от вашего зарегестрированного приложения на [сайте DonationAlerts](https://www.donationalerts.com/application/clients). Остальные значения оставьте по дефолту.

- Disconnect - Отключает текущие подключение к веб-сокету.
- Делегат OnDonateReceived - Срабатывает при получении доната.

## Дополнительно
[Официальная документация Donation Alerts API](https://www.donationalerts.com/apidoc)
<br>[Плагин для работы с Donation Alerts API от другого разработчика(Ufna)](https://github.com/ufna/DonationAlerts)
