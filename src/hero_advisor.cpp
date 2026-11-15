#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "hero_advisor.h"
#include "config.h"
#include <cmath>
#include <algorithm>
#include <random>

HeroAdvisor& HeroAdvisor::Instance() {
    static HeroAdvisor instance;
    return instance;
}

HeroAdvisor::HeroAdvisor() {
    InitHeroDatabase();
}

HeroAdvisor::~HeroAdvisor() {}

void HeroAdvisor::InitHeroDatabase() {
    // 1. Razor
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Разор";
        m.primaryRole = L"Мид / Оффлейн";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл на миде, Фейзы или ПТ, Стики, Фалкон Блейд";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"для скорости бега и урона под высасыванием" },
            { L"Маелшторм", 2700, "maelstrom", {"gleipnir", "mjollnir"}, L"для урона молниями и фарма пачек" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы не стоять в станах во время драки" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"для двойного шторма и сноса построек" },
            { L"Рефрешер", 5000, "refresher", {}, L"для двойного ультимейта" }
        };
        m.skillPriority = L"Макси второй скилл (высасывание урона) — высасывай весь урон у врага при каждом подходе к крипам! Докачивай первый скилл (кольцо молний), на шестом бери ульту.";
        m.laneStrategy = L"Держи связь второго скилла до конца, сбивай врагу здоровье и забирай лотусы из пруда каждые 3 минуты (или полторы в Турбо)!";
        metaDatabase_["razor"] = m;
    }

    // 2. Shadow Fiend (СФ / Nevermore)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Шадоу Финд (СФ)";
        m.primaryRole = L"Мид";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, ПТ и стики";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"сапог на ловкость для статов и переключения" },
            { L"Блинк Даггер", 2250, "blink", {"swift_blink", "arcane_blink"}, L"для мгновенного врыва и позиционки" },
            { L"Лотар (Shadow Blade)", 3000, "invis_sword", {"silver_edge"}, L"для невидимости, урона и эскейпа" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы кастовать Реквием без сбивания" },
            { L"Даедалус", 5100, "greater_crit", {}, L"для колоссальных критов с руки" }
        };
        m.skillPriority = L"Макси койлы (первый скилл) и пассивку на души. На шестом или девятом уровне бери ульту (Реквием).";
        m.laneStrategy = L"Зачищай линию койлами, фарми ближайшие нейтральные лагеря и забирай руны каждые 2 минуты!";
        metaDatabase_["nevermore"] = m;
        metaDatabase_["shadow_fiend"] = m;
    }

    // 3. Pudge
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Пудж";
        m.primaryRole = L"Мид / Оффлейн / Роум";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Фейзы, стики, Урна и Ботл";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"для брони и догона под вонью" },
            { L"Блинк Даггер", 2250, "blink", {"overwhelming_blink"}, L"для моментального хука в упор и ульты" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"огромная вонь, урон и снижение лечения врагам" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы тебя не прерывали во время поедания" },
            { L"Тараска", 5100, "heart", {}, L"для бесконечного здоровья и регенерации" }
        };
        m.skillPriority = L"Макси вонь (второй скилл) для замедления и фарма, хук на 1-2, на шестом обязательно ульту.";
        m.laneStrategy = L"Держись за союзными крипами, хукай под вышку и сразу жми вонь вместе с ультимейтом!";
        metaDatabase_["pudge"] = m;
    }

    // 4. Invoker
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Инвокер";
        m.primaryRole = L"Мид";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Урна, сапог и стики";
        m.progression = {
            { L"Вессел (большая урна)", 2800, "spirit_vessel", {}, L"убивать танков и снимать реген" },
            { L"Блинк Даггер", 2250, "blink", {}, L"для позиционки и комбо" },
            { L"БКБ", 4050, "black_king_bar", {}, L"защита от контроля в замесе" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"усиление заклинаний Cataclysm" }
        };
        m.skillPriority = L"В Квас-Векс билде качай Векс и Квас для Торнадо и ЕМП. В Экзорте качай Экзорт для Санстрайка.";
        m.laneStrategy = L"Следи за боковыми линиями и помогай союзникам Санстрайком по оглушенным целям!";
        metaDatabase_["invoker"] = m;
    }

    // 5. Juggernaut
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Джаггернаут";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию (бот за Сияние / топ за Тьму)";
        m.earlyItemsRu = L"Фейзы, стики, Врейт Бэнд";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"скорость бега и броня" },
            { L"Маелшторм", 2700, "maelstrom", {"bfury", "mjollnir", "gleipnir"}, L"быстрый фарм и прок молний в ульте" },
            { L"Манта", 4550, "manta", {"sange_and_yasha"}, L"сброс сайленса и урон копиями" },
            { L"Шард", 1400, "shard", {}, L"крутилка с ударами и уроном" },
            { L"Бабочка", 5450, "butterfly", {}, L"увороты, скорость атаки и ловкость" }
        };
        m.skillPriority = L"Макси крутилку (первый скилл) для фарма и киллов под саппорта, лечебный тотем на 1-2, на 6 ульту.";
        m.laneStrategy = L"Крутись во врагов вместе со станом саппорта, ставь тотем для отхила и не давай харассить себя!";
        metaDatabase_["juggernaut"] = m;
    }

    // 6. Phantom Assassin (ПА / Фантомка)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Фантомка (ПА)";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"ПТ, стики и Врейт Бэнд";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы и скорость атаки" },
            { L"Батлфури (БФ)", 4100, "bfury", {}, L"основа быстрого фарма леса и линий" },
            { L"Дезолятор", 3500, "desolator", {}, L"снижение брони и ваншоты с крита" },
            { L"БКБ", 4050, "black_king_bar", {}, L"без него тебя разорвут магией за секунду" },
            { L"Башер (Абиссал)", 2875, "basher", {"abyssal_blade"}, L"контроль врагов через БКБ" }
        };
        m.skillPriority = L"Кинжал на 2-3 для добивания крипов на линии, макси прыжок для фарма, блюр на 1, на 6 ульту.";
        m.laneStrategy = L"Фарми кинжалом издалека, держи блюр включенным чтобы не светиться на миникарте врага!";
        metaDatabase_["phantom_assassin"] = m;
    }

    // 7. Sniper
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Снайпер";
        m.primaryRole = L"Мид / Керри";
        m.laneType = L"мид";
        m.earlyItemsRu = L"ПТ, Врейт Бэнд и стики";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"скорость атаки и ловкость" },
            { L"Драгон Лэнс", 1900, "dragon_lance", {"hurricane_pike"}, L"огромная дальность стрельбы" },
            { L"Маелшторм", 2700, "maelstrom", {"mjollnir", "gleipnir"}, L"зачистка крипов молниями" },
            { L"БКБ", 4050, "black_king_bar", {}, L"спасение от врыва и фокуса" },
            { L"Даедалус", 5100, "greater_crit", {}, L"убойные криты на огромной дистанции" }
        };
        m.skillPriority = L"Качай шрапнель для контроля зоны и фарма, макси пассивку на дальность атаки. На 6 ульту.";
        m.laneStrategy = L"Держи дистанцию, кидай шрапнель под ноги врагу и добивай ультимейтом с расстояния!";
        metaDatabase_["sniper"] = m;
    }

    // 8. Anti-Mage (АМ)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Антимаг (АМ)";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"ПТ и стики";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы и выживаемость на линии" },
            { L"Батлфури (БФ)", 4100, "bfury", {}, L"главный артефакт для скоростного фарма леса" },
            { L"Манта", 4550, "manta", {}, L"сжигать ману иллюзиями и снимать сайленс" },
            { L"Башер", 2875, "basher", {"abyssal_blade"}, L"стан сквозь защиту от магии" },
            { L"Бабочка", 5450, "butterfly", {}, L"увороты и максимальный урон" }
        };
        m.skillPriority = L"Качай блинк на 3-4 для прыжков по нейтральным лагерям, сжигание маны на 2, щит отражения заклинаний.";
        m.laneStrategy = L"Пушь боковые линии, при опасности жми щит отражения и прыгай блинком в деревья!";
        metaDatabase_["antimage"] = m;
        metaDatabase_["anti_mage"] = m;
    }

    // 9. Faceless Void (Войд)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Войд";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"ПТ, стики и Врейт Бэнд";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"скорость атаки для пробития башей" },
            { L"Маелшторм", 2700, "maelstrom", {"mjollnir", "gleipnir"}, L"урон молниями внутри купола и фарм" },
            { L"Манта", 4550, "manta", {}, L"сброс контроля и выживаемость" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы не получить стан сквозь купол" },
            { L"Рефрешер", 5000, "refresher", {}, L"двойная Хроносфера — гарантированная победа" }
        };
        m.skillPriority = L"Прыжок назад во времени на 2, макси баши (пассивку), на шестом Хроносферу.";
        m.laneStrategy = L"Ставь купол так, чтобы закрыть ключевых героев врага, не заперев своих дальнобойных союзников!";
        metaDatabase_["faceless_void"] = m;
    }

    // 10. Drow Ranger (Тракса)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Тракса";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"ПТ, стики и Врейт Бэнд";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"ловкость и скорость" },
            { L"Драгон Лэнс", 1900, "dragon_lance", {"hurricane_pike"}, L"дальность стрельбы и статы" },
            { L"Манта", 4550, "manta", {}, L"сброс пыли и сайленса" },
            { L"БКБ", 4050, "black_king_bar", {}, L"защита от прыгающих в упор врагов" },
            { L"Бабочка", 5450, "butterfly", {}, L"огромная ловкость и пробитие брони" }
        };
        m.skillPriority = L"Макси ледяные стрелы для харасса и замедления, залп стрел на 2, на шестом ульту на пробитие брони.";
        m.laneStrategy = L"Держи дистанцию! Если враг подойдет вплотную — ульта отключится, отталкивай его сайленсом!";
        metaDatabase_["drow_ranger"] = m;
    }

    // 11. Slark (Сларк)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Сларк";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"ПТ, стики и Орб Коррозии";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"переключение на интеллект для первого скилла" },
            { L"Диффуза", 2500, "diffusal_blade", {"disperser"}, L"сжигание маны и замедление" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"два прыжка на огромную дистанцию" },
            { L"БКБ", 4050, "black_king_bar", {}, L"неуязвимость в бою вместе с ультимейтом" },
            { L"Скади", 5300, "skadi", {}, L"замедление и гора характеристик" }
        };
        m.skillPriority = L"Макси первый скилл (пакт) для фарма и сброса станов, прыжок на 2, пассивку на ловкость.";
        m.laneStrategy = L"Сбрасывай пактом вражеские замедления и контроли, набивай стаки ловкости и уходи в туман хилиться!";
        metaDatabase_["slark"] = m;
    }

    // 12. Wraith King (ВК)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"ВК (Леорик)";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"Фейзы, стики и перчатка силы";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"броня и догон врагов" },
            { L"Армлет", 2500, "armlet", {"radiance"}, L"огромный урон и выживаемость абузом армлета" },
            { L"Блинк Даггер", 2250, "blink", {}, L"мгновенный стан со старта замеса" },
            { L"Дезолятор", 3500, "desolator", {}, L"пробивание брони критами" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы не сожгли ману до перерождения" }
        };
        m.skillPriority = L"Макси криты и скелетов для быстрого фарма леса, стан на 1-2, на 6 бери перерождение.";
        m.laneStrategy = L"Вызывай скелетов в лес для параллельного фарма, следи за маной на ультимейт!";
        metaDatabase_["wraith_king"] = m;
        metaDatabase_["skeleton_king"] = m;
    }

    // 13. Sven (Свен)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Свен";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"ПТ, стики и брейсер";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"скорость атаки и сила" },
            { L"Эхо Сабля", 2500, "echo_sabre", {"harpoon"}, L"двойной удар и замедление" },
            { L"Блинк Даггер", 2250, "blink", {}, L"врыв в толпу с включенным ультимейтом" },
            { L"БКБ", 4050, "black_king_bar", {}, L"обязательно перед заходом в драку" },
            { L"Даедалус", 5100, "greater_crit", {}, L"криты по 1500 сплешем по всей команде" }
        };
        m.skillPriority = L"Макси сплеш (второй скилл) для пылесоса крипов, стан на 2, крик на броню, на 6 ульту.";
        m.laneStrategy = L"Включай ульт и крик ДО блинка, влетай с молотка и стирай саппортов за 2 удара!";
        metaDatabase_["sven"] = m;
    }

    // 14. Ursa (Урса)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Урса";
        m.primaryRole = L"Керри";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"Фейзы, стики и Орб Коррозии";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"броня и догон" },
            { L"Диффуза", 2500, "diffusal_blade", {"disperser"}, L"сжигать ману и не отпускать цель" },
            { L"Блинк Даггер", 2250, "blink", {}, L"мгновенная посадка на лицо врага" },
            { L"БКБ", 4050, "black_king_bar", {}, L"защита от замедлений и кайта" },
            { L"Башер", 2875, "basher", {"abyssal_blade"}, L"баш за серию лап" }
        };
        m.skillPriority = L"Макси третий скилл (нарастающий урон лапами) и второй (скорость ударов), один поинт в топот.";
        m.laneStrategy = L"Забирай раннего Рошана под ультимейтом и смоком на 15-й минуте!";
        metaDatabase_["ursa"] = m;
    }

    // 15. Morphling (Морф)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Морф";
        m.primaryRole = L"Керри / Мид";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"ПТ, стики и Врейт Бэнд";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы для перекачки" },
            { L"Драгон Лэнс", 1900, "dragon_lance", {}, L"дальность стрельбы и сила/ловкость" },
            { L"Манта", 4550, "manta", {}, L"сброс сайленса и урон копий" },
            { L"Скади", 5300, "skadi", {}, L"куча статов и замедление через БКБ" },
            { L"Бабочка", 5450, "butterfly", {"satanic"}, L"увороты и огромная ловкость" }
        };
        m.skillPriority = L"Макси волну (первый скилл) и адаптивный удар, перекачку держи аккуратно.";
        m.laneStrategy = L"Не перекачивайся в ноль ловкости если у врагов есть нюки, жми волну для сейва!";
        metaDatabase_["morphling"] = m;
    }

    // 16. Axe (Акс)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Акс";
        m.primaryRole = L"Оффлейн";
        m.laneType = L"сложную линию (топ за Сияние / бот за Тьму)";
        m.earlyItemsRu = L"Фейзы, стики и брейсер";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"броня и догон" },
            { L"Блинк Даггер", 2250, "blink", {"overwhelming_blink"}, L"основа инициации всего героя" },
            { L"Блейдмейл (обратка)", 2100, "blade_mail", {}, L"враги сами убивают себя об твой агр" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы не разорвали магией пока крутишься" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"снижение брони и урона врагов крутилкой" }
        };
        m.skillPriority = L"Макси крутилку (пассивку) и агр (первый скилл), на шестом бери топор (казнь).";
        m.laneStrategy = L"Прыгай с Блинка в толпу врагов, включай Блейдмейл и сразу жми первый скилл!";
        metaDatabase_["axe"] = m;
    }

    // 17. Legion Commander (Легионка / ЛК)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Легионка (ЛК)";
        m.primaryRole = L"Оффлейн";
        m.laneType = L"сложную линию";
        m.earlyItemsRu = L"Фейзы, стики и соул ринг";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"скорость и урон" },
            { L"Блейдмейл (обратка)", 2100, "blade_mail", {}, L"враг бьет тебя в дуэли и убивает сам себя" },
            { L"Блинк Даггер", 2250, "blink", {"overwhelming_blink"}, L"мгновенный прыжок на цель под дуэль" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы вражеские саппорты не дали контроль в дуэли" },
            { L"Дезолятор", 3500, "desolator", {}, L"пробивание брони с руки" }
        };
        m.skillPriority = L"Макси стрелы (первый скилл) для урона по пачкам и скорости бега, второй скилл на снятие станов. На 6 Дуэль.";
        m.laneStrategy = L"Комбо на дуэль: жми второй скилл и Блейдмейл, прыгай Блинком и сразу жми Дуэль!";
        metaDatabase_["legion_commander"] = m;
    }

    // 18. Mars (Марс)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Марс";
        m.primaryRole = L"Оффлейн";
        m.laneType = L"сложную линию";
        m.earlyItemsRu = L"Фейзы, Соул Ринг и стики";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"броня и урон" },
            { L"Соул Ринг", 805, "soul_ring", {}, L"бесконечная мана на копья" },
            { L"Блинк Даггер", 2250, "blink", {}, L"врыв для арены и прибивания к стене" },
            { L"БКБ", 4050, "black_king_bar", {}, L"свободное перемещение внутри арены" },
            { L"Рефрешер", 5000, "refresher", {"desolator"}, L"две арены и два копья подряд" }
        };
        m.skillPriority = L"Макси копье (первый скилл) для урона и стана, удар щитом на 2, пассивный щит на 1. На 6 Арену.";
        m.laneStrategy = L"Ставь Арену вокруг врагов и прибивай ключевого врага копьем прямо к забору Арены!";
        metaDatabase_["mars"] = m;
    }

    // 19. Centaur Warrunner (Кентавр)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Кентавр";
        m.primaryRole = L"Оффлейн";
        m.laneType = L"сложную линию";
        m.earlyItemsRu = L"Фейзы, стики и два брейсера";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"броня и догон" },
            { L"Блинк Даггер", 2250, "blink", {"overwhelming_blink"}, L"мгновенный прыжок и топот" },
            { L"Блейдмейл", 2100, "blade_mail", {"pipe"}, L"возврат урона врагам" },
            { L"Тараска", 5100, "heart", {}, L"колоссальный запас здоровья и регенерация" },
            { L"Шива", 4825, "shivas_guard", {}, L"снижение скорости атаки и хила врагов" }
        };
        m.skillPriority = L"Макси второй скилл (двойной удар) для размена здоровьем, пассивный возврат урона и топот. На 6 разгон всей команды.";
        m.laneStrategy = L"Жми ультимейт для спасения союзников или быстрого догона врагов через всю карту!";
        metaDatabase_["centaur"] = m;
    }

    // 20. Tidehunter (Тайд)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Тайдхантер";
        m.primaryRole = L"Оффлейн";
        m.laneType = L"сложную линию";
        m.earlyItemsRu = L"Фейзы, стики и соул ринг";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"броня и скорость" },
            { L"Блинк Даггер", 2250, "blink", {}, L"главный инструмент для каста Раваги" },
            { L"Шива", 4825, "shivas_guard", {}, L"ледяная волна и аура замедления" },
            { L"Рефрешер", 5000, "refresher", {}, L"двойная Равага на весь экран" }
        };
        m.skillPriority = L"Макси якорь (третий скилл) для фарма и срезки урона врагов, пассивный панцирь от физики. На 6 Равагу.";
        m.laneStrategy = L"Не держи Равагу вечно — если видишь 2-3 ключевых героев, прыгай и жми ульт!";
        metaDatabase_["tidehunter"] = m;
    }

    // 21. Magnus (Магнус)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Магнус";
        m.primaryRole = L"Оффлейн / Мид";
        m.laneType = L"сложную линию";
        m.earlyItemsRu = L"ПТ, стики и соул ринг";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы и скорость" },
            { L"Блинк Даггер", 2250, "blink", {}, L"прыжок для RP и вывоза врагов" },
            { L"Форс Стафф", 2200, "force_staff", {"harpoon"}, L"для выталкивания врагов под вышку" },
            { L"БКБ", 4050, "black_king_bar", {}, L"защита от сбивания RP" },
            { L"Рефрешер", 5000, "refresher", {}, L"двойной Reverse Polarity" }
        };
        m.skillPriority = L"Макси бафф на сплеш и урон (второй скилл) для фарма, удар волной на 2, один поинт в вывоз сквер. На 6 RP.";
        m.laneStrategy = L"Прыгай Блинком за спину врагам, жми RP и вывози их прямо под свою башню!";
        metaDatabase_["magnataur"] = m;
        metaDatabase_["magnus"] = m;
    }

    // 22. Primal Beast
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Праймал Бист";
        m.primaryRole = L"Мид / Оффлейн";
        m.laneType = L"сложную линию / мид";
        m.earlyItemsRu = L"Фейзы, стики и два брейсера";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"скорость и урон" },
            { L"Блейдмейл", 2100, "blade_mail", {}, L"возврат урона пока бегаешь по врагам" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы не сбили разгон и ультимейт" },
            { L"Кая-Саша", 4100, "kaya_and_sange", {}, L"усиление урона способностей и статус резист" }
        };
        m.skillPriority = L"Макси топот (второй скилл) и разгон (первый скилл). На шестом бери ульту (хватание врага об землю).";
        m.laneStrategy = L"Разгоняйся первым скиллом, включай топот и бегай прямо по врагам для огромного урона!";
        metaDatabase_["primal_beast"] = m;
    }

    // 23. Lina
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Лина";
        m.primaryRole = L"Мид / Керри";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, ПТ и стики";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"скорость атаки под пассивку" },
            { L"Маелшторм", 2700, "maelstrom", {"gleipnir"}, L"быстрый фарм и прок молний" },
            { L"БКБ", 4050, "black_king_bar", {}, L"выживание в драке" },
            { L"Даедалус", 5100, "greater_crit", {}, L"гигантский урон с руки" },
            { L"Сатаник", 5050, "satanic", {}, L"вампиризм на полное здоровье" }
        };
        m.skillPriority = L"Макси первый скилл (драконье пламя) и пассивку на скорость атаки. Один поинт в стан. На 6 ульту Лагуну.";
        m.laneStrategy = L"Поддерживай три стака пассивки спамом способностей и расстреливай врагов с руки!";
        metaDatabase_["lina"] = m;
    }

    // 24. Storm Spirit (Шторм)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Шторм Спирит";
        m.primaryRole = L"Мид";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, ПТ, стики и нулик";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы и переключение маны" },
            { L"Кая", 2050, "kaya", {"kaya_and_sange"}, L"снижение расхода маны на ультимейт" },
            { L"Орчид", 3475, "orchid", {"bloodthorn"}, L"сайленс и соло киллы по всей карте" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чтобы не поймали в контроль на выходе из шара" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"стяжка врагов вокруг себя во время врыва" }
        };
        m.skillPriority = L"Реминант на 1-2, макси пассивку на урон от заклинаний и стяжку. На 6 бери шаровую молнию.";
        m.laneStrategy = L"Летай на короткие дистанции для прока пассивки, следи за остатком маны!";
        metaDatabase_["storm_spirit"] = m;
    }

    // 25. Ember Spirit (Эмбер)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Эмбер Спирит";
        m.primaryRole = L"Мид";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, Фейзы и стики";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"броня и урон" },
            { L"Маелшторм", 2700, "maelstrom", {"gleipnir"}, L"прок молний со второго скилла по всей пачке" },
            { L"Мейдж Слейер", 2625, "mage_slayer", {}, L"снижение магического урона врагов" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чистый урон без контроля" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"5 ремнантов на огромную дистанцию" }
        };
        m.skillPriority = L"Макси щит от магии (третий скилл) и цепи (первый скилл), второй скилл на 1. На 6 ульту.";
        m.laneStrategy = L"Оставляй ремнант на базе или за вышкой для моментального сейва при опасности!";
        metaDatabase_["ember_spirit"] = m;
    }

    // 26. Void Spirit
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Войд Спирит";
        m.primaryRole = L"Мид";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, ПТ и стики";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы и скорость" },
            { L"Эхо Сабля", 2500, "echo_sabre", {"harpoon"}, L"двойной удар и замедление" },
            { L"Дезолятор", 3500, "desolator", {}, L"колоссальный физический урон с ультимейта" },
            { L"БКБ", 4050, "black_king_bar", {}, L"выживаемость в фокусе" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"два заряда сайленса на щите" }
        };
        m.skillPriority = L"Макси щит (второй скилл) для урона и поглощения, астрал на 2, один поинт в призрака. На 6 ульту.";
        m.laneStrategy = L"Влетай с ульты, давай щит, прячься в астрал и добивай вторым зарядом ультимейта!";
        metaDatabase_["void_spirit"] = m;
    }

    // 27. Puck (Пак)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Пак";
        m.primaryRole = L"Мид";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, ПТ и стики";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы и скорость" },
            { L"Вич Блейд", 2600, "witch_blade", {"parasme"}, L"яд, замедление и урон с руки" },
            { L"Блинк Даггер", 2250, "blink", {}, L"неуловимость через астрал и блинк" },
            { L"Еул", 2625, "cyclone", {"wind_waker"}, L"дополнительный сейв и сброс сайленса" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"атака сквозь спираль сна" }
        };
        m.skillPriority = L"Макси шар (первый скилл) и разлом с сайленсом. Один поинт в астрал. На шестом спираль сна.";
        m.laneStrategy = L"Бросай шар, уходи в астрал и телепортируйся на шар за мгновение до урона!";
        metaDatabase_["puck"] = m;
    }

    // 28. Queen of Pain (Квопа)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Квопа";
        m.primaryRole = L"Мид";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, ПТ и стики";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы" },
            { L"Вич Блейд", 2600, "witch_blade", {"parasme"}, L"яд и замедление с руки" },
            { L"Кая-Саша", 4100, "kaya_and_sange", {}, L"усиление урона способностей и выживаемость" },
            { L"БКБ", 4050, "black_king_bar", {}, L"чистый врыв без контроля" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"крик от кинжала по площади" }
        };
        m.skillPriority = L"Кинжал на 2 для харасса на линии, макси крик боли для зачистки пачек. Блинк на 2. На 6 ульту.";
        m.laneStrategy = L"Кидай кинжал по кулдауну и на шестом уровне делай соло-килл под крик и ультимейт!";
        metaDatabase_["queenofpain"] = m;
    }

    // 29. Tinker (Тинкер)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Тинкер";
        m.primaryRole = L"Мид / Саппорт";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, Соул Ринг и Арканы";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"запас маны" },
            { L"Блинк Даггер", 2250, "blink", {}, L"прыжки по деревьям" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"лазер по площади с уменьшением размера врагов" },
            { L"Шива", 4825, "shivas_guard", {}, L"урон и замедление вокруг" },
            { L"Хекс", 5675, "sheepstick", {}, L"бесконечный контроль по цели" }
        };
        m.skillPriority = L"Качай лазер и ракеты, на шестом бери ультимейт.";
        m.laneStrategy = L"Прыгай Блинком в деревья и держи дистанцию от станов врагов!";
        metaDatabase_["tinker"] = m;
    }

    // 30. Zeus (Зевс)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Зевс";
        m.primaryRole = L"Мид / Саппорт";
        m.laneType = L"мид";
        m.earlyItemsRu = L"Ботл, Арканы и стики";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"мана на молнии" },
            { L"Филактерия (Кханда)", 2400, "phylactery", {"khanda"}, L"дополнительный урон со второй кнопки" },
            { L"Шард", 1400, "shard", {}, L"прыжок небесный прыжок для побега и замедления" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"Нимбус — глобальная туча с молниями по всей карте" },
            { L"Рефрешер", 5000, "refresher", {}, L"двойной глобальный ультимейт" }
        };
        m.skillPriority = L"Молния на 1 для ластхита, макси второй скилл (болт) для огромного урона. На 6 ульту.";
        m.laneStrategy = L"Давай ультимейт по команде союзников, чтобы добить сбежавших врагов в тумане войны!";
        metaDatabase_["zeus"] = m;
        metaDatabase_["zuus"] = m;
    }

    // 31. Witch Doctor (Вич Доктор / ВД)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Вич Доктор";
        m.primaryRole = L"Саппорт (Поз 5)";
        m.laneType = L"легкую линию для помощи керри";
        m.earlyItemsRu = L"Арканы, стики и варды";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"мана для союзного керри и себя" },
            { L"Глиммер Кейп", 2150, "glimmer_cape", {}, L"невидимость и магический щит во время ульты" },
            { L"Шард", 1400, "shard", {}, L"кукла Вуду — спасение от смерти и урон" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"ульта отскакивает по всем врагам — переворот игры" },
            { L"БКБ", 4050, "black_king_bar", {}, L"ульта без возможности сбивания" }
        };
        m.skillPriority = L"Макси маледикт (третий скилл) и бочки (первый скилл), один поинт в хилку. На 6 Смертельный тотем.";
        m.laneStrategy = L"Кидай бочку когда рядом два врага или крип, вешай маледикт и сразу ставь тотем ультимейта!";
        metaDatabase_["witch_doctor"] = m;
    }

    // 32. Crystal Maiden (ЦМка)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"ЦМка (Crystal Maiden)";
        m.primaryRole = L"Саппорт (Поз 5)";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"Транквилы, стики и варды";
        m.progression = {
            { L"Транквилы", 925, "tranquil_boots", {"boots_of_bearing"}, L"скорость бега и постоянный отхил" },
            { L"Глиммер Кейп", 2150, "glimmer_cape", {}, L"невидимость во время каста ультимейта" },
            { L"Форс Стафф", 2200, "force_staff", {"hurricane_pike"}, L"выталкивание союзников и себя из ловушек" },
            { L"Шард", 1400, "shard", {}, L"скольжение и каст ультимейта на ходу" },
            { L"БКБ", 4050, "black_king_bar", {}, L"гарантия полного ультимейта в центре замеса" }
        };
        m.skillPriority = L"Качай колбу (первый скилл) и кристаллическую заморозку (второй скилл), пассивка на ауру маны на 2. На 6 ульту.";
        m.laneStrategy = L"Держи позицию за спиной керри, замораживай врагов под атаки союзника, покупай варды!";
        metaDatabase_["crystal_maiden"] = m;
    }

    // 33. Lion (Лион)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Лион";
        m.primaryRole = L"Саппорт (Поз 4 / 5)";
        m.laneType = L"линию";
        m.earlyItemsRu = L"Транквилы, стики и варды";
        m.progression = {
            { L"Транквилы", 925, "tranquil_boots", {}, L"скорость передвижения и реген здоровья" },
            { L"Блинк Даггер", 2250, "blink", {}, L"мгновенная инициация станом или хексом" },
            { L"Аганим Шард", 1400, "shard", {}, L"высасывание маны у 3 целей с иммунитетом к магии" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"ульта пальцем по области и накопление зарядов" },
            { L"Линза", 2275, "aether_lens", {}, L"дальность каста всех заклинаний" }
        };
        m.skillPriority = L"Макси стан (первый скилл) и высасывание маны, хекс на 1-2. На 6 Палец Смерти.";
        m.laneStrategy = L"Высасывай ману у вражеских оффлейнеров по кулдауну, добивай Пальцем для накопления урона!";
        metaDatabase_["lion"] = m;
    }

    // 34. Shadow Shaman (Шаман / Раста)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Раста (Шаман)";
        m.primaryRole = L"Саппорт (Поз 5)";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"Арканы, стики и варды";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"мана на прокаст" },
            { L"Блинк Даггер", 2250, "blink", {}, L"прыжок с хексом и сеткой" },
            { L"Аганим Шард", 1400, "shard", {}, L"змейки при связывании сеткой" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"змейки бьют по двум целям со сплешем" },
            { L"Рефрешер", 5000, "refresher", {}, L"двойной спавн змеек для моментального сноса трона" }
        };
        m.skillPriority = L"Макси молнии (первый скилл) для урона, хекс и сетку по 2 поинта. На 6 Змеек.";
        m.laneStrategy = L"Ставь змеек прямо под вражескую башню когда союзники отогнали врагов — сноси строения!";
        metaDatabase_["shadow_shaman"] = m;
    }

    // 35. Ogre Magi (Огр Маг)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Огр Маг";
        m.primaryRole = L"Саппорт / Оффлейн";
        m.laneType = L"линию";
        m.earlyItemsRu = L"Арканы, стики и брейсер";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"запас маны под мультикаст" },
            { L"Мидас", 2200, "hand_of_midas", {}, L"мультикаст на 4 крипа — тонна золота и опыта" },
            { L"Блинк Даггер", 2250, "blink", {}, L"врыв для стана" },
            { L"Бладстоун (Тараска)", 4400, "bloodstone", {"heart"}, L"огромная живучесть от силы" }
        };
        m.skillPriority = L"Макси второй скилл (воспламенение) для харасса, стан на 2, бафф на скорость атаки. На 6 Мультикаст.";
        m.laneStrategy = L"Иди размениваться тычками на первом уровне — у тебя куча брони и здоровья, вешай бафф на керри!";
        metaDatabase_["ogre_magi"] = m;
    }

    // 36. Jakiro (Джакиро)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Джакиро";
        m.primaryRole = L"Саппорт (Поз 5)";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"Арканы, стики и варды";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"мана на спам пламени" },
            { L"Еул", 2625, "cyclone", {}, L"комбо: поднял в ветра — положил ледяную стенку точно под падение" },
            { L"Форс Стафф", 2200, "force_staff", {}, L"спасение союзников" },
            { L"Аганим Шард", 1400, "shard", {}, L"процентный урон по вышкам и героям" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"ледяное пламя пробивающее БКБ" }
        };
        m.skillPriority = L"Макси дыхание огня (первый скилл) и жидкое пламя (третий скилл), ледяной путь на 2. На 6 ульту.";
        m.laneStrategy = L"Жги вражеские вышки третьим скиллом при каждом подходе крипов!";
        metaDatabase_["jakiro"] = m;
    }

    // 37. Rubick (Рубик)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Рубик";
        m.primaryRole = L"Саппорт (Поз 4)";
        m.laneType = L"сложную линию";
        m.earlyItemsRu = L"Арканы, стики и варды";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"мана на ворованные заклинания" },
            { L"Блинк Даггер", 2250, "blink", {}, L"мгновенный телекинез и врыв с ультимейтом" },
            { L"Линза", 2275, "aether_lens", {}, L"дальность каста всех украденных спеллов" },
            { L"Аганим Шард", 1400, "shard", {}, L"бросок союзников и себя телекинезом" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"хранение двух украденных заклинаний сразу" }
        };
        m.skillPriority = L"Макси зеленые молнии (второй скилл) для урона и снижения урона врагов, телекинез на 2. На 6 воровство.";
        m.laneStrategy = L"Держись сзади, выжидай каста ключевых ультимейтов врага (Равага, Черная дыра) и кради их!";
        metaDatabase_["rubick"] = m;
    }

    // 38. Dazzle (Даззл)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Даззл";
        m.primaryRole = L"Саппорт (Поз 5)";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"Арканы, стики и варды";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"запас маны" },
            { L"Глиммер Кейп", 2150, "glimmer_cape", {}, L"невидимость и магический щит" },
            { L"Шард", 1400, "shard", {}, L"превращение яда в хекс по области" },
            { L"Форс Стафф", 2200, "force_staff", {}, L"дистанция для креста" }
        };
        m.skillPriority = L"Яд на 2 для харасса, макси волну хила, крест на 1-2. На 6 ультимейт.";
        m.laneStrategy = L"Давай крест в последнюю секунду жизни союзного керри и сразу вливай волну лечения!";
        metaDatabase_["dazzle"] = m;
    }

    // 39. Disruptor (Дисраптор)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Дисраптор";
        m.primaryRole = L"Саппорт (Поз 5)";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"Арканы, стики и варды";
        m.progression = {
            { L"Арканы", 1300, "arcane_boots", {}, L"мана на прокаст" },
            { L"Глиммер Кейп", 2150, "glimmer_cape", {}, L"сейв союзников" },
            { L"Блинк Даггер", 2250, "blink", {}, L"позиция для возвращения врагов" },
            { L"Аганим Шард", 1400, "shard", {}, L"усиление электро-клетки" },
            { L"Аганим", 4200, "ultimate_scepter", {}, L"ультимейт глушит предметы (отключает БКБ)" }
        };
        m.skillPriority = L"Молния на 1-2 для вижена, макси возврат во времени (второй скилл) и забор (третий). На 6 Бурю.";
        m.laneStrategy = L"Возвращай врагов которые телепортировались на вышку назад на базу, ставь клетку и ульт!";
        metaDatabase_["disruptor"] = m;
    }

    // 40. Spirit Breaker (Бара)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Бара (Spirit Breaker)";
        m.primaryRole = L"Оффлейн / Поз 4";
        m.laneType = L"сложную линию";
        m.earlyItemsRu = L"Фейзы, стики и соул ринг";
        m.progression = {
            { L"Фейзы", 1500, "phase_boots", {}, L"скорость бега для урона башей" },
            { L"Лотар (Shadow Blade)", 3000, "invis_sword", {"silver_edge"}, L"разгон в невидимости сквозь варды" },
            { L"Октарин", 5200, "octarine_core", {}, L"кулдаун разгона каждые несколько секунд" },
            { L"БКБ", 4050, "black_king_bar", {}, L"неостановимый разгон в драке" },
            { L"Тараска", 5100, "heart", {}, L"неубиваемость" }
        };
        m.skillPriority = L"Макси разгон (первый скилл) и пассивку на баши с уроном от скорости. На 6 ульту.";
        m.laneStrategy = L"Разгоняйся на вражеских саппортов по кулдауну, сбивай тп и ломай позиционку!";
        metaDatabase_["spirit_breaker"] = m;
    }

    // 41. Muerta (Муэрта)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Муэрта";
        m.primaryRole = L"Керри / Мид";
        m.laneType = L"легкую линию / мид";
        m.earlyItemsRu = L"ПТ, стики и Врейт Бэнд";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"скорость атаки и ловкость" },
            { L"Драгон Лэнс", 1900, "dragon_lance", {"hurricane_pike"}, L"дальность стрельбы" },
            { L"Маелшторм", 2700, "maelstrom", {"gleipnir"}, L"быстрый фарм и двойной прок" },
            { L"БКБ", 4050, "black_king_bar", {}, L"защита от магии пока ты в призрачной форме" },
            { L"Даедалус", 5100, "greater_crit", {}, L"безумный урон под ультимейтом" }
        };
        m.skillPriority = L"Макси выстрел рикошетом (первый скилл) и пассивку на двойной выстрел. Круг духов на 1. На 6 ульту.";
        m.laneStrategy = L"Рикошеть выстрелом от крипов во вражеского героя под вышкой, нажимай ульт когда враг врубил БКБ!";
        metaDatabase_["muerta"] = m;
    }

    // 42. Kez (Кез)
    {
        HeroBuildMeta m;
        m.heroDisplayName = L"Кез";
        m.primaryRole = L"Керри / Мид";
        m.laneType = L"легкую линию";
        m.earlyItemsRu = L"ПТ, стики и Орб Коррозии";
        m.progression = {
            { L"ПТ", 1400, "power_treads", {}, L"статы и скорость" },
            { L"Эхо Сабля", 2500, "echo_sabre", {"harpoon"}, L"замедление и двойной удар стойки" },
            { L"Манта", 4550, "manta", {}, L"сброс дебаффов и урон" },
            { L"БКБ", 4050, "black_king_bar", {}, L"выживаемость в бою" },
            { L"Бабочка", 5450, "butterfly", {}, L"ловкость и увороты" }
        };
        m.skillPriority = L"Качай обе боевые стойки, используй катану для урона и кинжалы для мобильности. На 6 ульту.";
        m.laneStrategy = L"Переключай стойки в нужный момент: режь катаной в упор и уходи кинжалами при фокусе!";
        metaDatabase_["kez"] = m;
    }
}

HeroBuildMeta HeroAdvisor::FindHeroMeta(const std::string& rawName) {
    std::string key = rawName;
    for (auto& c : key) c = (char)tolower(c);

    // Strip "npc_dota_hero_"
    const std::string prefix = "npc_dota_hero_";
    if (key.rfind(prefix, 0) == 0) {
        key = key.substr(prefix.length());
    }

    // Direct match
    auto it = metaDatabase_.find(key);
    if (it != metaDatabase_.end()) return it->second;

    // Substring match
    for (auto& pair : metaDatabase_) {
        if (key.find(pair.first) != std::string::npos || pair.first.find(key) != std::string::npos) {
            return pair.second;
        }
    }

    // Intelligent dynamic fallback for any other unlisted hero:
    HeroBuildMeta fallback;
    fallback.heroDisplayName = (key.empty()) ? L"Твой герой" : std::wstring(key.begin(), key.end());
    fallback.primaryRole = L"Линия";
    fallback.laneType = L"линию";
    fallback.earlyItemsRu = L"Сапог, стики и расходники на линию";
    fallback.progression = {
        { L"Улучшенный сапог (ПТ или Фейзы)", 1450, "boots", {"power_treads", "phase_boots", "arcane_boots"}, L"для скорости и статов" },
        { L"Ключевой предмет (Блинк, Молнии или Глиммер)", 2500, "blink", {"maelstrom", "glimmer_cape", "blade_mail", "diffusal_blade", "force_staff"}, L"основа для драк и фарма" },
        { L"БКБ (защиту от магии)", 4050, "black_king_bar", {}, L"чтобы свободно действовать в замесе" },
        { L"Аганим или лейт-предмет", 4200, "ultimate_scepter", {"heart", "shivas_guard", "butterfly", "greater_crit"}, L"усиление героя на позднюю стадию" }
    };
    fallback.skillPriority = L"Макси основную боевую способность для харасса и добивания крипов, а на шестом обязательно бери ультимейт!";
    fallback.laneStrategy = L"Контролируй крипов, забирай лотусы из пруда каждые 3 минуты (или полторы в Турбо) и следи за картой!";
    return fallback;
}

bool HeroAdvisor::HasItem(const BuildItem& item) {
    if (currentItems_.empty()) return false;

    for (const auto& rawItem : currentItems_) {
        std::string lowerItem = rawItem;
        for (auto& c : lowerItem) c = (char)tolower(c);
        if (lowerItem == "empty" || lowerItem.empty()) continue;

        // Check primary identifier
        if (!item.idSub.empty() && lowerItem.find(item.idSub) != std::string::npos) {
            return true;
        }

        // Check upgrades/alternates
        for (const auto& alt : item.altSubs) {
            if (!alt.empty() && lowerItem.find(alt) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

std::wstring HeroAdvisor::DetectLane(int x, int y) {
    // Map is roughly -8000 to +8000
    // Mid lane is along x ≈ y diagonal: abs(x - y) < 2600 and within central zone
    if (std::abs(x - y) < 2600 && std::abs(x) < 4500 && std::abs(y) < 4500) {
        return L"мид";
    }
    if (y > 1500 && x < 1500) {
        return L"топ (верхнюю линию)";
    }
    if (x > 1500 && y < 1500) {
        return L"бот (нижнюю линию)";
    }
    return L"линию";
}

void HeroAdvisor::Reset() {
    currentHeroName_ = "";
    currentGold_ = 0;
    currentLevel_ = 0;
    posX_ = 0;
    posY_ = 0;
    clockTime_ = 0;
    currentItems_.clear();
    abilities_.clear();
    detectedLane_ = L"";
    laneAnnounced_ = false;
    lastAffordableItem_ = "";
    lastNotifiedLevel_ = 0;
}

void HeroAdvisor::UpdateGameState(
    const std::string& heroName,
    int gold,
    int heroLevel,
    int xpos,
    int ypos,
    int clockTime,
    const std::vector<std::string>& currentItems,
    const std::vector<std::pair<std::string, int>>& abilities
) {
    currentHeroName_ = heroName;
    currentGold_ = gold;
    currentLevel_ = heroLevel;
    posX_ = xpos;
    posY_ = ypos;
    clockTime_ = clockTime;
    currentItems_ = currentItems;
    abilities_ = abilities;

    // Detect lane during early game
    if (clockTime_ >= 0 && clockTime_ <= 600 && !laneAnnounced_) {
        std::wstring l = DetectLane(xpos, ypos);
        if (l == L"мид" || l == L"топ (верхнюю линию)" || l == L"бот (нижнюю линию)") {
            detectedLane_ = l;
        }
    }
}

bool HeroAdvisor::CheckProactiveAdvice(std::wstring& outAdvice) {
    if (currentHeroName_.empty()) return false;
    HeroBuildMeta meta = FindHeroMeta(currentHeroName_);
    bool toxic = ConfigManager::Get().enableToxicMode;

    // 1. Proactive Lane Announcement (when match starts and player arrives on lane)
    if (!laneAnnounced_ && clockTime_ >= 10 && clockTime_ <= 180 && !detectedLane_.empty()) {
        laneAnnounced_ = true;
        if (toxic) {
            outAdvice = L"Здарова, братан! Вижу, ты на «" + meta.heroDisplayName + L"» приполз на " + detectedLane_ + L"! " +
                L"Смотри не нафидь в первые пять минут! " + meta.laneStrategy;
        } else {
            outAdvice = L"Здарова, братух! Вижу, ты на герое «" + meta.heroDisplayName + L"» пошел на " + detectedLane_ + L"! Газяву открой и давай по красоте: " +
                meta.laneStrategy;
        }
        return true;
    }

    // 2. Proactive Affordable Core Item Notification
    // When the player accumulates enough gold for their next target item
    for (const auto& item : meta.progression) {
        if (!HasItem(item)) {
            if (currentGold_ >= item.cost && lastAffordableItem_ != item.idSub) {
                lastAffordableItem_ = item.idSub;
                if (toxic) {
                    outAdvice = L"У тебя уже " + std::to_wstring(currentGold_) + L" золота! Хватает на " + item.nameRu +
                        L" (" + std::to_wstring(item.cost) + L" монет)! Быстро жми курьера, пока враги тебя не прирезали и половину не отжали!";
                } else {
                    outAdvice = L"Братан, у тебя уже " + std::to_wstring(currentGold_) + L" монет — как раз хватает на " + item.nameRu +
                        L" (" + std::to_wstring(item.cost) + L" золота)! Заказывай курьером, ща разваливать пойдем!";
                }
                return true;
            }
            break;
        }
    }

    return false;
}

std::wstring HeroAdvisor::GetBuyAdvice() {
    HeroBuildMeta meta = FindHeroMeta(currentHeroName_);
    bool toxic = ConfigManager::Get().enableToxicMode;

    // Early game (< 5 mins) before boots
    BuildItem targetItem;
    BuildItem nextItem;
    bool foundTarget = false;

    for (size_t i = 0; i < meta.progression.size(); ++i) {
        if (!HasItem(meta.progression[i])) {
            targetItem = meta.progression[i];
            foundTarget = true;
            if (i + 1 < meta.progression.size()) {
                nextItem = meta.progression[i + 1];
            }
            break;
        }
    }

    if (!foundTarget) {
        if (toxic) {
            return L"Ты уже упакован по полной! Жми байбек, покупай Муншард или Рапиру и сноси трон, не тяни время!";
        } else {
            return L"Все ключевые предметы уже собраны! Копи на байбек, покупай Муншард или расходуй Аганим.";
        }
    }

    // Compare player's current gold with targetItem.cost
    if (currentGold_ >= targetItem.cost) {
        if (toxic) {
            std::wstring res = L"У тебя " + std::to_wstring(currentGold_) + L" золота в кармане горит! Солить его собрался?! " +
                L"Прямо сейчас заказывай " + targetItem.nameRu + L" (" + std::to_wstring(targetItem.cost) + L" монет)! " +
                L"А то тебя в лесу поймают и половину отберут!";
            if (!nextItem.nameRu.empty()) {
                res += L" И сразу готовься копить на " + nextItem.nameRu + L"!";
            }
            return res;
        } else {
            std::wstring res = L"Братух, у тебя " + std::to_wstring(currentGold_) + L" монет — прямо сейчас хватает на " +
                targetItem.nameRu + L" (" + std::to_wstring(targetItem.cost) + L" золота)! Заказывай курьером прямо сейчас!";
            if (!nextItem.nameRu.empty()) {
                res += L" А следующим предметом начни собирать " + nextItem.nameRu + L".";
            }
            return res;
        }
    } else {
        int need = targetItem.cost - currentGold_;
        if (toxic) {
            return L"Руки себе купи! А если по делу — целься в " + targetItem.nameRu + L" (" + targetItem.roleReason +
                L", стоит " + std::to_wstring(targetItem.cost) + L"). У тебя сейчас всего " + std::to_wstring(currentGold_) +
                L" монет. Иди нормально крипов добивай, осталось " + std::to_wstring(need) + L" золота!";
        } else {
            return L"Братан, сейчас собирай " + targetItem.nameRu + L" (" + targetItem.roleReason +
                L", цена " + std::to_wstring(targetItem.cost) + L"). У тебя " + std::to_wstring(currentGold_) +
                L" монет, осталось подфармить " + std::to_wstring(need) + L" золота! Я в тебя верю!";
        }
    }
}

std::wstring HeroAdvisor::GetSkillAdvice() {
    HeroBuildMeta meta = FindHeroMeta(currentHeroName_);
    bool toxic = ConfigManager::Get().enableToxicMode;

    if (toxic) {
        return L"Голову себе вкачай! А по скиллам на герое «" + meta.heroDisplayName + L"»: " +
            meta.skillPriority + L" Хватит кнопки невпопад нажимать!";
    } else {
        return L"Братух, по скиллам на «" + meta.heroDisplayName + L"» делай так: " + meta.skillPriority + L" Раскатаем их!";
    }
}

std::wstring HeroAdvisor::GetGeneralAdvice() {
    HeroBuildMeta meta = FindHeroMeta(currentHeroName_);
    bool toxic = ConfigManager::Get().enableToxicMode;

    if (toxic) {
        return L"Ты на «" + meta.heroDisplayName + L"»! Не тупи: " + meta.laneStrategy +
            L" И качай правильно: " + meta.skillPriority;
    } else {
        return L"Ты на герое «" + meta.heroDisplayName + L"» (" + meta.primaryRole + L"). " +
            meta.laneStrategy + L"\n" + meta.skillPriority;
    }
}

std::wstring HeroAdvisor::GetDeathRoast() {
    static const wchar_t* roasts[] = {
        L"Ты чё, уснул на клавиатуре?! Тебя крипы быстрее убьют, чем ты кнопку нажмешь! Сиди в таверне отдыхай.",
        L"Гениальный фид, братан! Враги тебе уже в общий чат спасибо пишут. Зачем полез в соло без вижена?!",
        L"Марат, это просто шедевр позиционки! Помер как бот на ровном месте. Нажимай байбек или не позорься!",
        L"Туда тебя и надо! В следующий раз карту открой перед тем как в кусты бежать!",
        L"Минус игрок! Ты вообще кнопки нажимать умеешь, или мне самому за тебя сесть играть?!"
    };
    int idx = rand() % 5;
    return roasts[idx];
}

std::wstring HeroAdvisor::GetKillComment(int streak) {
    bool toxic = ConfigManager::Get().enableToxicMode;
    if (toxic) {
        if (streak >= 5) {
            return L"РАМПАГА БЛИЗКО! ДОБИВАЙ ИХ, ЧЁ ТЫ МЯМЛИШЬ, СНОСИ ТРОН!";
        } else if (streak >= 3) {
            return L"О, проснулся! Наконец-то фраг сделал, я думал ты чисто за компанию в катку зашел.";
        } else {
            return L"Красава, Марат! Хоть раз не опозорился, шевели клешнями дальше!";
        }
    } else {
        if (streak >= 5) {
            return L"РАМПАГА БЛИЗКО! Продолжай давить!";
        } else {
            return L"Красава, Марат!";
        }
    }
}

std::wstring HeroAdvisor::GetFriendlyChat() {
    static const wchar_t* chats[] = {
        L"Здарова, братух! Газяву взял с собой? Садись поудобнее, ща раскидаем этих бедолаг!",
        L"Здарова, братан! Чё как настрой? Сегодня чисто на победку играем, главное не тильтовать!",
        L"О, здорово, брат! Я на связи. Если не знаешь чё брать — жми F6, раскидаю по красоте!",
        L"Салам, братишка! Газяву открыл? Главное на клавиатуру не пролей, а то скиллы зальешь!",
        L"Братух, не забудь чайник поставить, пока крипы не пошли! Мы тут надолго засели.",
        L"Здарова, родной! Я с тобой, держу кулаки за твой лайн. Раскатаем их в щепки!",
        L"Братан, расслабься, кайфуй от игры. Лейт наш, главное спокойно фармить!",
        L"Здарова, братуха! Ну чё, готов сегодня апнуть птс без единого луза?"
    };
    int idx = rand() % 8;
    return chats[idx];
}

std::wstring HeroAdvisor::GetLotusComment(bool isTurbo) {
    bool toxic = ConfigManager::Get().enableToxicMode;
    if (toxic) {
        return L"Лотус в пруду заспавнился! Метнулся кабанчиком и забрал, пока враги на халяву не сожрали!";
    } else {
        return L"Братух, лотус созрел в пруду! Сбегай забери халявное здоровье и ману.";
    }
}

std::wstring HeroAdvisor::GetJoke() {
    static const wchar_t* jokes[] = {
        L"Знаешь, почему Пудж никогда не играет в прятки? Потому что его вонь выдает на всю карту!",
        L"Слушай дотерский анекдот: заходят как-то в бар мидер, керри и саппорт... а саппорта не пустили, у него вардов не было!",
        L"Шутка минутки: твой керри сказал, что сейчас зафармит лес и подключится. Катка закончилась 20 минут назад!",
        L"Спрашивают у Инвокера: сколько кнопок тебе нужно для счастья? Он говорит: все 27, и ещё три на клавиатуре союзника!",
        L"Знаешь, что общего между лотусом и девушкой? Если вовремя не придешь — заберёт другой!",
        L"Купил мужик Рапиру, а она ему как раз. Приходит на мид — а там уже трон упал!",
        L"Учительница спрашивает Вовочку: кем ты хочешь стать? Вовочка: лейтером! Учительница: почему? Вовочка: чтобы 40 минут ничего не делать, а потом сказать, что команда раки!",
        L"Дотер приходит к окулисту: Доктор, мне кажется, за мной следят! Окулист: Спокойно, батенька, это просто Баратрум нажал разгон!"
    };
    int idx = rand() % 8;
    return jokes[idx];
}

std::wstring HeroAdvisor::GetHowAreYou() {
    bool toxic = ConfigManager::Get().enableToxicMode;
    if (toxic) {
        static const wchar_t* toxicReplies[] = {
            L"У меня-то всё отлично! А вот твой крипстат плакать заставляет. Соберись давай!",
            L"Да нормально, сижу в углу и смотрю как ты крипов мимо кассы пропускаешь. Работай давай!",
            L"Бывало и лучше, если б ты на линии так не подставлялся! Иди фарми, не отвлекайся!"
        };
        return toxicReplies[rand() % 3];
    } else {
        static const wchar_t* friendlyReplies[] = {
            L"По кайфу, братуха! Сижу считаю твою голду, жду когда начнем разваливать кабинеты! А ты как сам?",
            L"Отлично, братан! Голда капает, свинка хрюкает, жизнь налаживается. Ты сам как настрой?",
            L"Да всё четко, брат! Главное чтобы мидер не тильтовал. Газяву открыл? Погнали тащить!",
            L"На чилле, родной! Я с тобой, слежу за закупом. Давай покажем им мастер-класс!"
        };
        return friendlyReplies[rand() % 4];
    }
}

std::wstring HeroAdvisor::GetThanksReaction() {
    bool toxic = ConfigManager::Get().enableToxicMode;
    if (toxic) {
        static const wchar_t* toxicThanks[] = {
            L"Спасибо на хлеб не намажешь! Трон иди ломай лучше!",
            L"Да ладно тебе, главное в следующем файте не слейся как нуб!",
            L"Всегда пожалуйста! Хоть кто-то в этой катке слушает умную свинку."
        };
        return toxicThanks[rand() % 3];
    } else {
        static const wchar_t* broThanks[] = {
            L"От души, братуха! Мы с тобой банда, ща всю карту перевернем!",
            L"Да брось, братан, я просто подсказываю, а тащишь-то ты! Красава!",
            L"Всегда пожалуйста, родной! Всегда на связи, ради победы хоть кабанчиком метнусь!",
            L"Вместе мы сила, братуха! Давай додавим этих бедолаг!"
        };
        return broThanks[rand() % 4];
    }
}

std::wstring HeroAdvisor::GetCheerUp() {
    bool toxic = ConfigManager::Get().enableToxicMode;
    if (toxic) {
        return L"Хватит ныть и плакать! Собери сопли в кулак, нажми кнопки нормально и выиграй этот файт! Камбэк реален!";
    } else {
        static const wchar_t* cheers[] = {
            L"Братух, без паники! В Доте камбэки случаются каждую вторую катку. Соберись, один хороший файт — и трон наш!",
            L"Не тильтуй, братан! Купи БКБ, встань за спину и нажми кнопки. Они сами ошибутся!",
            L"Да пофиг на эти ошибки, играй в свою игру! Лейт за нами, главное не сливать байбеки!"
        };
        return cheers[rand() % 3];
    }
}

std::wstring HeroAdvisor::GetWakePrompt() {
    static const wchar_t* prompts[] = {
        L"Да, братуха? Я слушаю! Что подсказать?",
        L"На связи, братан! Говори, я в деле!",
        L"Чё как, родной? Я тебя слышу, спрашивай!",
        L"Я тут, братишка! Подсказать закуп или пошутить?"
    };
    return prompts[rand() % 4];
}
