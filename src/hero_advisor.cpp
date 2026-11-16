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
        L"Где твой мьюинг?! Тебя крип опустил на глазах у всего тиктока! 🤫🧏‍♂️",
        L"Помер как скуф без пива и альтушки! Сиди в таверне, тапай хомяка!",
        L"1000 минус 7... Жми байбек, дед инсайд недоделанный! Я умер прости...",
        L"Дубайский шоколад изо рта выпал от такого удара! Зачем полез один?!",
        L"Это не смерть, это байт на комменты в тиктоке! Но кринжа навалил знатно.",
        L"Ты чё, уснул на клавиатуре?! Тебя крипы быстрее убьют, чем ты кнопку нажмешь! Сиди отдыхай.",
        L"Гениальный фид, братан! Враги тебе уже в общий чат спасибо пишут. Зачем полез в соло без вижена?!",
        L"Марат, это просто шедевр позиционки! Помер как бот на ровном месте. Нажимай байбек или не позорься!",
        L"ПАСХАЛКО! Медный бык на миду разогрет, а ты уже на фонтане загораешь!",
        L"Скибиди нокаут! Враг отправил тебя в реки тиктока отдыхать!"
    };
    int idx = rand() % 10;
    return roasts[idx];
}

std::wstring HeroAdvisor::GetKillComment(int streak) {
    bool toxic = ConfigManager::Get().enableToxicMode;
    std::wstring name = ConfigManager::Get().playerName;
    if (name.empty()) name = L"братуха";

    if (toxic) {
        if (streak >= 10) {
            static const wchar_t* t10[] = {
                L"ХОЛИ ШИТ! ДЕСЯТЫЙ В РЯД! Вражеский саппорт удалил Доту и плачет!",
                L"СТРИК ДЕСЯТЬ! Ты сегодня не человек, ты адская мясорубка! ЧИНАЗЕС СЮДААА!",
                L"ДЕСЯТКА БЕЗ СМЕРТЕЙ! Сноси трон, бедолаги сдались морально!",
                L"НЕРЕАЛЬНЫЙ БОГ ДОТЫ! 10 фрагов под фонк! Ломай им базу с ноги!"
            };
            int n = sizeof(t10) / sizeof(t10[0]);
            return t10[rand() % n];
        } else if (streak >= 7) {
            static const wchar_t* t7[] = {
                L"ГОДЛАЙК! Седьмой фраг в ряд! Враги даже с базы боятся выйти!",
                L"МОНСТЕР КИЛЛ! Ты их просто стираешь в порошок, это тяжелый люкс!",
                L"СЕМЬ ТЕЛ В ТАВЕРНУ! Чисто сигма геймплей, продолжай унижать!",
                L"СЕМЕРКА! Вражеский мидер уже пишет GG в чат, добивай их!"
            };
            int n = sizeof(t7) / sizeof(t7[0]);
            return t7[rand() % n];
        } else if (streak >= 5) {
            static const wchar_t* t5[] = {
                L"АНСТОППАБЛ! Пять фрагов подряд! Неудержимый мясник на сервере!",
                L"МЕГА КИЛЛ! Пятый бедолага отправился отдыхать! ЧИНАЗЕС СЮДААА!",
                L"ПЯТЕРА В РЯД! Свинья в экстазе! Забирай Рошана и заканчивай клоунаду!",
                L"ПЯТЫЙ ФРАГ! Разваливаешь как босс КФС 52, чисто в соло!",
                L"РАМПАГА БЛИЗКО! Пять трупов, дави до конца под бразильский фонк!",
                L"УЖЕ ПЯТЬ! Где их команда вообще? Ты играешь против крипов!"
            };
            int n = sizeof(t5) / sizeof(t5[0]);
            return t5[rand() % n];
        } else if (streak >= 3) {
            static const wchar_t* t3[] = {
                L"ТРИПЛ СТРИК! О, проснулся чилл гай! Наконец-то фраг сделал, чисто сигма мув!",
                L"КИЛЛИНГ СПРИ! Три фрага без смертей! Хоть раз порадовал свина!",
                L"Тройной стрик! Нащупал кнопки на клавиатуре, продолжай в том же духе!"
            };
            int n = sizeof(t3) / sizeof(t3[0]);
            return t3[rand() % n];
        } else {
            static const wchar_t* t1[] = {
                L"Красава! Хоть раз не опозорился, это тяжелый люкс!",
                L"Минус один бедолага! Надеюсь, это не случайность!",
                L"Фраг забрал, а крипов кто добивать будет? Работай давай!",
                L"Один есть! Не расслабляйся, катка сама себя не выиграет!"
            };
            int n = sizeof(t1) / sizeof(t1[0]);
            return t1[rand() % n];
        }
    } else {
        // Friendly / Hype mode with tiered streaks
        if (streak >= 10) {
            static const wchar_t* f10[] = {
                L"АБСОЛЮТНЫЙ БОГ ДОТЫ! ДЕСЯТЬ В РЯД! Чиназес сюдааа, это хайлайт года!",
                L"ХОЛИ ШИТ! ДЕСЯТЫЙ ФРАГ ПОД ФОНК! Тебя уже репортят за читы, чисто машина!",
                L"10 КИЛЛОВ БЕЗ СМЕРТЕЙ! Залетай в реки тиктока с двух ног, трон падает сам!",
                L"ДЕСЯТКА! Невероятный стрик! Ты лучший игрок на планете, горжусь тобой!"
            };
            int n = sizeof(f10) / sizeof(f10[0]);
            return f10[rand() % n];
        } else if (streak >= 7) {
            static const wchar_t* f7[] = {
                L"БОГОПОДОБЕН! СТРИК СЕМЬ ФРАГОВ! Свинья аплодирует стоя!",
                L"МОНСТЕР КИЛЛ! Седьмой фраг в копилку! Разваливаешь на легком вайбе!",
                L"СЕМЬ В РЯД БЕЗ ЕДИНОЙ ОШИБКИ! Вот это тяжелый люкс, вот это уровень!",
                L"СЕМЕРКА! Враги в тильте, а мы на чилле и мьюинге сносим базу!"
            };
            int n = sizeof(f7) / sizeof(f7[0]);
            return f7[rand() % n];
        } else if (streak >= 5) {
            static const std::wstring f5[] = {
                L"МЕГА КИЛЛ! Пятый подряд! Рампага уже стучится в двери!",
                L"АНСТОППАБЛ! Пять фрагов подряд! Ты неудержим, братишка, дави их!",
                L"ПЯТЫЙ В ТАВЕРНУ! Чиназес сюдааа! Раздаешь стильно и со вкусом!",
                L"УЖЕ ПЯТЬ В РЯД! Красава, " + name + L"! Свинка на твоей стороне!",
                L"ПЯТЕРА! Чисто сигма геймплей под фонк! Погнали ломать трон!",
                L"РАМПАГА БЛИЗКО! Пять тел на кладбище! Забирай Рошана и заканчивай!",
                L"Пять киллов в ряд! Да ты сегодня на кураже, дубайский шоколад открывай!"
            };
            int n = sizeof(f5) / sizeof(f5[0]);
            return f5[rand() % n];
        } else if (streak >= 3) {
            static const wchar_t* f3[] = {
                L"КИЛЛИНГ СПРИ! Тройной стрик, пошла жара!",
                L"Три в ряд! Разминаешься перед рампагой, красава!",
                L"Уже три фрага без смертей! Чилловый парень разошелся не на шутку!",
                L"ТРИПЛ СТРИК! Сигма мув, минус очередная жертва!",
                L"Хороший темп! Уже три фрага, доминируй на карте!"
            };
            int n = sizeof(f3) / sizeof(f3[0]);
            return f3[rand() % n];
        } else {
            static const wchar_t* f1[] = {
                L"Чиназес сюдааа! Минус бедолага!",
                L"Чисто сигма мув, раскидал в соло!",
                L"Тяжелый люкс! Минус один!",
                L"Красава, родной! Залетаем в реки тиктока!",
                L"Хрю! Делаешь вещи под фонк!",
                L"Красивый фраг! Враг даже не понял, откуда прилетело!",
                L"Чилловый килл! Забирай фарм и держи темп!",
                L"Минус один! Свинья довольна, продолжай в том же духе!"
            };
            int n = sizeof(f1) / sizeof(f1[0]);
            return f1[rand() % n];
        }
    }
}

std::wstring HeroAdvisor::GetFriendlyChat() {
    static const wchar_t* chats[] = {
        L"Здарова, братух! Газяву взял? Я просто чилловый парень, ща раскидаем этих бедолаг!",
        L"Чиназес сюдааа! Братан, чё как настрой? Сегодня чисто на победку под бразильский фонк!",
        L"О, здорово, брат! Я на связи. Мьюинг держу, газяву пью, за твоим мидом слежу!",
        L"Салам, братишка! Открывай дубайский шоколад, устраивайся поудобнее, ща будет шоу!",
        L"52 Санкт-Петербург! Я босс КФС и свинка в одном лице, готовься ломать трон!",
        L"Здарова, родной! Не будь скуфом, тапай крипов как хомяка, лейт сто процентов наш!",
        L"Здарова, сигма! Спокойствие и тяжелый люкс — наш девиз на эту катку!",
        L"Привет, братуха! Готовься, сегодня наш геймплей залетит в реки тиктока!"
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

void HeroAdvisor::InitJokeQueue() {
    const int TOTAL_JOKES = 12000;
    jokeQueue_.resize(TOTAL_JOKES);
    for (int i = 0; i < TOTAL_JOKES; ++i) {
        jokeQueue_[i] = i;
    }
    std::mt19937 g(1337 + (unsigned int)GetTickCount());
    std::shuffle(jokeQueue_.begin(), jokeQueue_.end(), g);
    currentJokeIndex_ = 0;
    jokeQueueInitialized_ = true;
}

std::wstring HeroAdvisor::GenerateJokeById(int id) {
    // 1. Handcrafted classic curated Dota jokes (first 40)
    static const wchar_t* classicJokes[] = {
        L"Знаешь, почему Пудж никогда не играет в прятки? Потому что его вонь выдает на всю карту!",
        L"Слушай дотерский анекдот: заходят как-то в бар мидер, керри и саппорт... а саппорта не пустили, у него вардов не было!",
        L"Шутка минутки: твой керри сказал, что сейчас зафармит лес и подключится. Катка закончилась 20 минут назад!",
        L"Спрашивают у Инвокера: сколько кнопок тебе нужно для счастья? Он говорит: все 27, и ещё три на клавиатуре союзника!",
        L"Знаешь, что общего между лотусом и девушкой? Если вовремя не придешь — заберёт другой!",
        L"Купил мужик Рапиру, а она ему как раз. Приходит на мид — а там уже трон упал!",
        L"Учительница спрашивает Вовочку: кем ты хочешь стать? Вовочка: лейтером! Учительница: почему? Вовочка: чтобы 40 минут ничего не делать, а потом сказать, что команда раки!",
        L"Дотер приходит к окулисту: Доктор, мне кажется, за мной следят! Окулист: Спокойно, батенька, это просто Баратрум нажал разгон!",
        L"Антимаг заходит в церковь, батюшка окропляет его святой водой, а Антимаг кричит: А-а-а, магия! И блинкуется через забор!",
        L"Знаешь, почему Снайпер всегда стреляет с другого конца карты? Боится в глаза тиммейтам посмотреть за такой пик!",
        L"Встречаются два СФа в миду: один с койла промахнулся, второй паузу отжал, а крипы уже трон догрызают!",
        L"Течис закопал 30 мин на Рошпите, сидит ждёт полчаса. Приходит курьер с вардом и разминирует весь его бизнес-план!",
        L"Войд поставил купол на пятерых... своих! А потом в общий чат пишет: Парни, не благодарите, я засейвил вас от урона!",
        L"Сларк прыгает с хайграунда: Паунс в окно! А там дерево, стан и пять репортов в профиль!",
        L"ЦМка гордо купила Вард и Сапог на 28 минуте. Керри: Ты где голду взяла? ЦМка: Рошана в соло заковыряла!",
        L"Энигма даёт Блэкхол в одного крипа. Команда орёт: Зачем?! Энигма: Этот крип смотрел на меня с осуждением!",
        L"Лион ультует пальцем в иллюзию Фантомки с криком: Получай, тварь! А сзади стоит настоящая с рапирой...",
        L"Зевс нажимает ульту на фонтане: Я внёс импакт! А у врагов пайп, аегис и фулл хп.",
        L"Бристлбек повернулся задом к врагам, а там Акс с криком: Сюда иди, родной! И крутится как стиралка на отжиме!",
        L"Шейкер прыгает в толпу, нажимает Эхослэм... а у врагов у всех БКБ. Шейкер: Пацаны, я просто салют запустил!",
        L"Тинкер нажал реарм 18 раз за секунду, клавиатура загорелась, соседи вызвали пожарных, а он всё ещё на базе!",
        L"Алхимик фармит 40 минут, варит Аганимы всей команде. Саппорт: Спасибо! Алхимик: С вас по 500 рублей на карту!",
        L"Кунка кидает Торрент, ставит Крест, пускает Корабль... и всё мимо! Команда: Куда?! Кунка: Я так вижу, я художник!",
        L"Урса заходит к Рошану на первой минуте, а Рошан ему говорит: Опять ты? Возьми сыр сам и уходи, надоел!",
        L"Хускар прыгает на лоу-хп врага, горит, орёт, а у врага обратка сработала. Два трупа и ноль профита!",
        L"Мипо приходит в паспортный стол: Мне нужно пять паспортов! Сотрудница: Зачем? Мипо: Нас тут пятеро, но зарплата одна!",
        L"Чен собрал зоопарк в лесу, идёт ломать вышку, а вражеский Свен нажал сплеш и сделал шашлык за две секунды!",
        L"Рубик украл у Пуджа Хук, хукает союзного Пуджа, Пудж хукает Рубика... так они и изобрели вечный двигатель в Доте!",
        L"Джаггернаут крутится в телепорт, думает неуязвимый. Приходит Баратрум с башем сквозь крутилку и отправляет в таверну!",
        L"Легионка даёт дуэль саппорту, саппорт нажимает Гост, а Легионка 5 секунд грустно смотрит в пустоту с дуэльным уроном!",
        L"Дум ультует в крипа: Зато этот крип больше никогда не использует свои мерзкие способности!",
        L"Рики сидит в инвизе 30 минут, смотрит как его команду выносят ногами вперёд: Зато у меня ноль смертей, пацаны!",
        L"Мортра прыгает с дагером, вылетает крит на 3000 в курьера. Мортра: Курьер уничтожен, задача выполнена!",
        L"Витч Доктор ставит маледикт, включает ульту: Потанцуем! А враги просто сделали шаг в сторону за кустик.",
        L"Вайпер плюнул во врага, враг плюнул в Вайпера... так на миду и образовалось болото с нулевым онлайном.",
        L"Дроу Рейнджер кричит: Не подходите ко мне близко! А к ней подкрался Сларк и обнял от души.",
        L"ОД прячет врага в астрал прямо перед ультой Лины: Лина: Спасибо за сейв врага, бро! ОД: Я помогал как мог!",
        L"Пак нажал шифт, думал перехитрил вселенную, а из кустов вылетает стан Свена размером с холодильник!",
        L"Свен под годс стренгом бьёт по вышке: Бум! А вышка под глифом. Свен сломал топор и ушёл плакать в лес.",
        L"Фантом Лансер создал 50 иллюзий, комп сгорел у всех десятерых игроков одновременно, катка завершилась ничьей!"
    };

    if (id < 40) {
        return classicJokes[id];
    }

    // 2. Procedural Combinatorial Joke Generator (IDs 40 to 12,000+)
    static const wchar_t* heroes[] = {
        L"Пудж", L"Инвокер", L"Антимаг", L"Снайпер", L"Мипо", L"Акс", L"Войд", L"Рики",
        L"Мортра", L"Сларк", L"Баратрум", L"Чен", L"Рубик", L"Тинкер", L"Бристлбек", L"Легионка",
        L"Джаггернаут", L"Лион", L"Зевс", L"Шторм Спирит", L"Энигма", L"Эртшейкер", L"Урса", L"Хускар",
        L"Клинкз", L"Алхимик", L"Баунти Хантер", L"Вивер", L"Кункка", L"Некрофос", L"Дум", L"Оутворлд Дестроер",
        L"Дроу Рейнджер", L"Свен", L"Тини", L"Витч Доктор", L"Пак", L"Фантом Лансер", L"Шадоу Финд", L"Вайпер"
    };

    static const wchar_t* items[] = {
        L"Рапиру", L"Мидас", L"БКБ", L"Дагон пятого уровня", L"Тараску", L"Блинки", L"Баттлфури",
        L"Радианс", L"Аганим", L"Шард", L"Линку", L"Этернал", L"Бабочку", L"Дезолятор", L"Кирасу"
    };

    int heroIdx = (id * 7 + 13) % 40;
    int hero2Idx = (id * 11 + 17) % 40;
    if (hero2Idx == heroIdx) hero2Idx = (heroIdx + 1) % 40;
    int itemIdx = (id * 13 + 5) % 15;
    int tmplType = (id / 40) % 16;

    std::wstring h1 = heroes[heroIdx];
    std::wstring h2 = heroes[hero2Idx];
    std::wstring itm = items[itemIdx];

    switch (tmplType) {
        case 0:
            return L"Приходит " + h1 + L" на приём к психотерапевту и жалуется: Доктор, меня никто в катке не уважает! Доктор: А вы пробовали нажать " + itm + L"? " + h1 + L": Доктор, вы что, с ума сошли?! Это же на победу, я так не играю!";
        case 1:
            return L"Разговаривают два дотера на хайграунде. Первый: Прикинь, наш " + h1 + L" купил " + itm + L" на 45-й минуте! Второй: И что, помогло? Первый: Ага, враги от смеха клавиатуры чаем залили и трон отдали!";
        case 2:
            return L"Учительница в школе дотеров спрашивает: Дети, кто расскажет стишок? Встаёт " + h1 + L": Тише, мыши, кот на крыше, а наш керри в лес ушёл, сорок крипов там нашёл! Сел на трон, заплакал он!";
        case 3:
            return L"Окулист осматривает " + h1 + L" и вздыхает: Батенька, у вас редчайшая форма слепоты! Вы в упор не видите вражеские варды, миникарту и союзный трон!";
        case 4:
            return L"Полиция Доты останавливает " + h1 + L" на речке: Гражданин, почему у вас " + itm + L" без документов и чека? " + h1 + L": Товарищ сержант, мамой клянусь, на Рошпите нашёл, враги сами выронили!";
        case 5:
            return L"Заходят как-то в таверну " + h1 + L" и " + h2 + L". Бармен спрашивает: Вам чего налить? " + h1 + L": Мне голды побольше, а вот этому господину — пять репортов за руин лайна!";
        case 6:
            return L"Объявление на Авито: Продаётся клавиатура, владелец — мейнер " + h1 + L". Кнопки Q, W, E, R и BKB даже муха не трогала, зато кнопки чата и паузы стёрты до пластмассы!";
        case 7:
            return L"Служба доверия Доты: Здравствуйте, меня зовут " + h1 + L", и я уже сорок минут не могу нажать БКБ! Оператор: Спокойно, дышите глубже, вы не одиноки, 90% вашего рейтинга страдают тем же!";
        case 8:
            return L"Встречаются в лесу крип и " + h1 + L". Крип смотрит с ужасом, а " + h1 + L" промахивается ластхитом три раза подряд! Крип: Братан, давай я сам добьюсь, мне на твои мучения смотреть больно!";
        case 9:
            return L"Рекрутер на собеседовании: Назовите вашу главную сильную сторону. Соискатель: Я умею терпеть любую боль! Рекрутер: Кем работали? Соискатель: Саппортом у " + h1 + L" на 2000 MMR!";
        case 10:
            return L"Знаешь, в чём разница между метеоритом и ультой " + h1 + L"? Метеорит хотя бы раз в миллион лет попадает в цель!";
        case 11:
            return L"Сидит " + h1 + L" на фонтане и плачет. Подходит " + h2 + L": Ты чего ревёшь? " + h1 + L": Да я купил " + itm + L", а тиммейты говорят, что я руинер! " + h2 + L": Не плачь, они правы!";
        case 12:
            return L"Ученые провели эксперимент: посадили нейросеть играть против " + h1 + L". Через 15 минут нейросеть заблокировала чат, продала все слоты и ушла майнить криптовалюту!";
        case 13:
            return L"Звонок в техподдержку Steam: Алло, у меня критический баг! Я нажал кнопки на " + h1 + L", а враг не умер! Поддержка: Попробуйте в следующий раз целиться мышкой в монитор, а не в потолок!";
        case 14:
            return L"Поймал " + h1 + L" золотую рыбку на лотусе. Рыбка: Отпусти меня, исполню любое желание! " + h1 + L": Хочу, чтобы " + h2 + L" не фидил! Рыбка: Слушай, давай лучше мир во всём мире, это реальнее!";
        default:
            return L"Секрет победы от " + h1 + L": сначала фармишь 30 минут " + itm + L", потом дерёшься 2 секунды без байбека, а в конце игры громко кричишь в войс, что виноваты саппорты!";
    }
}

std::wstring HeroAdvisor::GetJoke() {
    if (!jokeQueueInitialized_ || jokeQueue_.empty()) {
        InitJokeQueue();
    }
    if (currentJokeIndex_ >= jokeQueue_.size()) {
        std::mt19937 g((unsigned int)GetTickCount());
        std::shuffle(jokeQueue_.begin(), jokeQueue_.end(), g);
        currentJokeIndex_ = 0;
    }
    int jokeId = jokeQueue_[currentJokeIndex_++];
    return GenerateJokeById(jokeId);
}

std::wstring HeroAdvisor::GetHowAreYou() {
    bool toxic = ConfigManager::Get().enableToxicMode;
    if (toxic) {
        static const wchar_t* toxicReplies[] = {
            L"У меня-то всё отлично! А вот твой крипстат плакать заставляет. Соберись давай!",
            L"Да нормально, сижу в углу и смотрю как ты крипов мимо кассы пропускаешь. Работай давай!",
            L"Бывало и лучше, если б ты на линии так не подставлялся! Иди фарми, не отвлекайся!",
            L"Сижу считаю сколько раз ты мимо БКБ нажал. Пока счёт не в твою пользу, исправляйся!",
            L"Нормально, жду когда у тебя руки из плеч расти начнут, а не из бедра!",
            L"Да как сам... Твой винрейт меня до депрессии доведёт, давай тащи уже!",
            L"Смотрю на твои мувы и думаю: свиньи в космос летали, а ты крипа добить не можешь!",
            L"Пойдёт! Главное чтобы ты в следующем файте не изображал мешок с картошкой!"
        };
        return toxicReplies[rand() % 8];
    } else {
        static const wchar_t* friendlyReplies[] = {
            L"По кайфу, братуха! Сижу считаю твою голду, жду когда начнем разваливать кабинеты! А ты как сам?",
            L"Отлично, братан! Голда капает, свинка хрюкает, жизнь налаживается. Ты сам как настрой?",
            L"Да всё четко, брат! Главное чтобы мидер не тильтовал. Газяву открыл? Погнали тащить!",
            L"На чилле, родной! Я с тобой, слежу за закупом. Давай покажем им мастер-класс!",
            L"Превосходно, братишка! Вижу позитив, чувствую победу. Ща соберём слоты и сломаем трон!",
            L"Бодрячком, братух! Карта под контролем, варды горят, голда звенит. Настроение боевое!",
            L"Живём, кайфуем! Слежу за курьером и твоим ХП. Мы с тобой идеальная команда!",
            L"Полный порядок, братка! Солнце светит, крипы падают, в кармане шекели. Рвём их!"
        };
        return friendlyReplies[rand() % 8];
    }
}

std::wstring HeroAdvisor::GetThanksReaction() {
    bool toxic = ConfigManager::Get().enableToxicMode;
    if (toxic) {
        static const wchar_t* toxicThanks[] = {
            L"Спасибо на хлеб не намажешь! Трон иди ломай лучше!",
            L"Да ладно тебе, главное в следующем файте не слейся как нуб!",
            L"Всегда пожалуйста! Хоть кто-то в этой катке слушает умную свинку.",
            L"Не благодари, лучше купи варды или нажми БКБ вовремя!",
            L"Ага, спасибо! А трон сам себя не сломает, шевели клешнями!"
        };
        return toxicThanks[rand() % 5];
    } else {
        static const wchar_t* broThanks[] = {
            L"От души, братуха! Мы с тобой банда, ща всю карту перевернем!",
            L"Да брось, братан, я просто подсказываю, а тащишь-то ты! Красава!",
            L"Всегда пожалуйста, родной! Всегда на связи, ради победы хоть кабанчиком метнусь!",
            L"Вместе мы сила, братуха! Давай додавим этих бедолаг!",
            L"По-братски, родной! Твой успех — моя радость, газуй дальше!",
            L"Всегда к твоим услугам, бро! Красиво играешь, любо-дорого смотреть!"
        };
        return broThanks[rand() % 6];
    }
}

std::wstring HeroAdvisor::GetCheerUp() {
    bool toxic = ConfigManager::Get().enableToxicMode;
    if (toxic) {
        static const wchar_t* toxicCheers[] = {
            L"Хватит ныть и плакать! Собери сопли в кулак, нажми кнопки нормально и выиграй этот файт! Камбэк реален!",
            L"Тильт — для слабаков! Пока трон жив, игра не окончена. Бери мышку и работай!",
            L"Слёзы вытер, БКБ купил и пошёл ломать лица! Никаких отмазок!"
        };
        return toxicCheers[rand() % 3];
    } else {
        static const wchar_t* cheers[] = {
            L"Братух, без паники! В Доте камбэки случаются каждую вторую катку. Соберись, один хороший файт — и трон наш!",
            L"Не тильтуй, братан! Купи БКБ, встань за спину и нажми кнопки. Они сами ошибутся!",
            L"Да пофиг на эти ошибки, играй в свою игру! Лейт за нами, главное не сливать байбеки!",
            L"Братишка, выдохни! Сделай глоток газявы. Мы ещё покажем им, кто здесь батя!",
            L"Всё нормально, родной! Дота — игра ошибок, ща они подставятся под вышкой и мы перевернём игру!"
        };
        return cheers[rand() % 5];
    }
}

std::wstring HeroAdvisor::GetWakePrompt() {
    static const wchar_t* prompts[] = {
        L"Да, братуха? Я слушаю! Что подсказать?",
        L"На связи, братан! Говори, я в деле!",
        L"Чё как, родной? Я тебя слышу, спрашивай!",
        L"Я тут, братишка! Подсказать закуп или пошутить?",
        L"Слушаю во все уши, бро! Какой вопрос?",
        L"Да, мой капитан! Готов разваливать вместе с тобой!"
    };
    return prompts[rand() % 6];
}

std::wstring HeroAdvisor::GetMemeResponse(const std::wstring& trigger) {
    if (trigger == L"chill_guy") {
        static const wchar_t* memes[] = {
            L"Я просто чилловый парень, братух. Катка горит, трон трещит, а я сижу в свинарнике на чилле и кайфую.",
            L"Не напрягайся, я чилл гай! Вражеский Пудж хукает, а я руки в карманы и просто иду дальше.",
            L"Брат, будь как чилл гай: не тильтуй, крипы сами себя не зафармят, а победа будет за нами."
        };
        return memes[rand() % 3];
    }
    if (trigger == L"dubai_chocolate") {
        static const wchar_t* memes[] = {
            L"Где мой дубайский шоколад?! Хрустит фисташковой пастой и тестом катаифи! Купи мне плитку за 4000 голды вместо Рапиры!",
            L"Ооо, дубайский шоколад! Хруст на весь лес! Пока ты мид пушишь, я фисташковую пасту ложками наворачиваю.",
            L"Братух, если выиграем эту катку — ты обязан заказать мне дубайский шоколад прямо в виджет!"
        };
        return memes[rand() % 3];
    }
    if (trigger == L"kfc_boss") {
        static const wchar_t* memes[] = {
            L"Я босс КФС! Дай пять! 52 Санкт-Петербург, залетаем на Рошпит с ведром острых крыльев!",
            L"Тише будь! С тобой говорит босс КФС! Ща зажарим этих крипов как сочные стрипсы!",
            L"52! Братуха, это база! Ломаем трон и идём за баскетом дуэт!"
        };
        return memes[rand() % 3];
    }
    if (trigger == L"hamster") {
        static const wchar_t* memes[] = {
            L"Тапай, тапай хомяка! Листинг уже вот-вот! Ты зачем крипов тапаешь, ты хомяка тапай!",
            L"Братан, сколько у тебя прибыль в час на хомяке? Если меньше трёх лямов — в Доту даже не заходи!",
            L"Я натапал хомяка на целый Дагон пятого уровня! Правда, после листинга хватило только на танго..."
        };
        return memes[rand() % 3];
    }
    if (trigger == L"skibidi") {
        static const wchar_t* memes[] = {
            L"Скибиди доп доп доп ес ес! Скибиди дабл ю дип дип! Свиной десант на легкую линию!",
            L"Ты чё, словил брейнрот в тиктоке в 4 утра? Скибиди туалет в миду хукает крипов!",
            L"Скибиди свинка на связи! У врагов ноль ризза, у нас сто процентов скибиди ауры!"
        };
        return memes[rand() % 3];
    }
    if (trigger == L"sigma") {
        static const wchar_t* memes[] = {
            L"Врубаю бразильский фонк! Я сигма свин! Делаю фирменный взгляд сигмы и несу трон в соло без саппортов!",
            L"Сигма правило номер 52: никогда не извиняйся за украденный килл у керри! Сигма забирает всё!",
            L"Чисто сигма мув, братан! Пока они ругаются в войсе, ты молча ломаешь лица под фонк."
        };
        return memes[rand() % 3];
    }
    if (trigger == L"altushka") {
        static const wchar_t* memes[] = {
            L"Где моя альтушка с Госуслуг?! Заявление подал ещё на прошлой неделе! Сижу как скуф, пью чай и жду пуджа.",
            L"Братух, не будь скуфом! Диван, танки и пиво подождут — у нас тут битва за 2000 ммр!",
            L"Выдайте этой свинье сертификат на альтушку! Я в соло выиграл мид, имею полное право!"
        };
        return memes[rand() % 3];
    }
    if (trigger == L"paskhalko") {
        static const wchar_t* memes[] = {
            L"ПАСХАЛКО! Включаем вентиляторы! Медный бык на миду разогрет до предела!",
            L"Пасхалко, братулек! Ты заметил пасхалку? Вражеский керри купил мом на 30 минуте — вот это пасхалко!"
        };
        return memes[rand() % 2];
    }
    if (trigger == L"chinazes") {
        static const wchar_t* memes[] = {
            L"Ооо, сюдааа! Чиназес, братулек! Чисто саунтрес, кайф, отдых, газява течёт рекой!",
            L"Чиназес сюдааа! Вот это килл, вот это я понимаю уровень игры!"
        };
        return memes[rand() % 2];
    }
    if (trigger == L"mewing") {
        static const wchar_t* memes[] = {
            L"🤫🧏‍♂️ Тссс! Свинка на жестком мьюинге! Не сбивай мой джолайн своими вопросами, я держу осанку!",
            L"Я делаю мьюинг уже сорок минут. Чувствуешь эту острую челюсть? Даже крипы пугаются!"
        };
        return memes[rand() % 2];
    }
    if (trigger == L"lux") {
        static const wchar_t* memes[] = {
            L"Это тяжелый люкс, братан! Бабочка, Сатаник, Рапира — тихий олд мани свиной стиль!",
            L"Мы не просто играем в Доту, мы транслируем тяжелый люкс и уверенность в каждом крипе."
        };
        return memes[rand() % 2];
    }
    if (trigger == L"kefteme") {
        static const wchar_t* memes[] = {
            L"Что за тяги такие бархатные, фуфтыкели?! Ооо, кефтеме! Купил Трэвела третьего уровня!",
            L"Тяги бархатные, свиные копытца на максималках, летим по лайну быстрее ветра!"
        };
        return memes[rand() % 2];
    }
    if (trigger == L"dead_inside") {
        static const wchar_t* memes[] = {
            L"1000 минус 7... 993... Я гуль SSS ранга, я умер прости... Паузу отжал, койл дал, трон сломал!",
            L"Дед инсайд свинка в здании! Волосы белые, на шее канеки кен, в кармане два танго."
        };
        return memes[rand() % 2];
    }
    if (trigger == L"tiktok") {
        static const wchar_t* memes[] = {
            L"Братух, этот килл сто процентов залетит в реки тиктока под фонк на три миллиона просмотров!",
            L"Хватит скроллить тиктоки между смертями! У тебя крипы под вышкой плачут!",
            L"Подписывайтесь, ставьте лайки, пишите комменты! Сегодня стримим победу свиньи!"
        };
        return memes[rand() % 3];
    }
    if (trigger == L"goyda") {
        return L"ГОООЙДААА! Свиной полк идёт в наступление на Рошпит!";
    }
    return GetRandomTikTokMeme();
}

std::wstring HeroAdvisor::GetRandomTikTokMeme() {
    static const wchar_t* triggers[] = {
        L"chill_guy", L"dubai_chocolate", L"kfc_boss", L"hamster",
        L"skibidi", L"sigma", L"altushka", L"paskhalko",
        L"chinazes", L"mewing", L"lux", L"kefteme",
        L"dead_inside", L"tiktok", L"goyda"
    };
    int idx = rand() % 15;
    return GetMemeResponse(triggers[idx]);
}
