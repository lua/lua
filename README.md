-- MonetLoader for Android 2.4.1

-- Reference script: MVD Helper

script_name('MVD Helper')

script_version('1.5')

script_version_number(1)

script_author('1')

script_description('A jastin.')

local imgui = require 'mimgui' -- подключаем библиотеку мимгуи



local encoding = require 'encoding' -- подключаем библиотеку для работы с разными кодировками

encoding.default = 'CP1251' -- задаём кодировку по умолчанию

local u8 = encoding.UTF8 -- это позволит нам писать задавать названия/текст на кириллице



require 'widgets' -- подключить айди виджетов

local sampev = require 'samp.events' -- samp events для подмены синхры

local sf = require 'sampfuncs' -- sampfuncs для айди пакетов



local crok  = imgui.new.int(0)

local ctatys  = imgui.new.int(0)

local poziv = imgui.new.int(0)  -- Инициализируем переменную poziv

local medca1 = imgui.new.bool()

local medca2 = imgui.new.bool()

local poziv = imgui.new.int(0)

local aidi = imgui.new.int(0)

local rank = imgui.new.int(0)

local cena = imgui.new.int(0)

local cena1 = imgui.new.int(0)

local kolvo = imgui.new.int(0)







local SCREEN_W, SCREEN_H = getScreenResolution() -- константы размера экрана

local new = imgui.new -- создаём короткий псевдоним для удобства

local window_state = new.bool() -- создаём буффер для открытия окна

local window_scale = new.float(1.0) -- переменная для изменения размера окна, для удобства



-- небольшой стиль для красоты

function imgui.Theme()

  imgui.SwitchContext()

  style = imgui.GetStyle()



  --==[ STYLE ]==--

  style.WindowPadding = imgui.ImVec2(5, 5)

  style.FramePadding = imgui.ImVec2(10, 10)

  style.ItemSpacing = imgui.ImVec2(10, 10)

  style.ItemInnerSpacing = imgui.ImVec2(5, 5)

  style.TouchExtraPadding = imgui.ImVec2(5 * MONET_DPI_SCALE, 5 * MONET_DPI_SCALE)

  style.IndentSpacing = 0

  style.ScrollbarSize = 20 * MONET_DPI_SCALE

  style.GrabMinSize = 20 * MONET_DPI_SCALE



  --==[ BORDER ]==--

  style.WindowBorderSize = 1

  style.ChildBorderSize = 1

  style.PopupBorderSize = 1

  style.FrameBorderSize = 1

  style.TabBorderSize = 1



  --==[ ROUNDING ]==--

  style.WindowRounding = 5

  style.ChildRounding = 5

  style.FrameRounding = 5

  style.PopupRounding = 5

  style.ScrollbarRounding = 5

  style.GrabRounding = 5

  style.TabRounding = 5



  --==[ ALIGN ]==--

  style.WindowTitleAlign = imgui.ImVec2(0.5, 0.5)

  style.ButtonTextAlign = imgui.ImVec2(0.5, 0.5)

  style.SelectableTextAlign = imgui.ImVec2(0.5, 0.5)

  

  --==[ COLORS ]==--

  style.Colors[imgui.Col.Text]                   = imgui.ImVec4(1.00, 1.00, 1.00, 1.00)

  style.Colors[imgui.Col.TextDisabled]           = imgui.ImVec4(0.50, 0.50, 0.50, 1.00)

  style.Colors[imgui.Col.WindowBg]               = imgui.ImVec4(0.07, 0.07, 0.07, 1.00)

  style.Colors[imgui.Col.ChildBg]                = imgui.ImVec4(0.07, 0.07, 0.07, 1.00)

  style.Colors[imgui.Col.PopupBg]                = imgui.ImVec4(0.07, 0.07, 0.07, 1.00)

  style.Colors[imgui.Col.Border]                 = imgui.ImVec4(0.25, 0.25, 0.26, 0.54)

  style.Colors[imgui.Col.BorderShadow]           = imgui.ImVec4(0.00, 0.00, 0.00, 0.00)

  style.Colors[imgui.Col.FrameBg]                = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.FrameBgHovered]         = imgui.ImVec4(0.25, 0.25, 0.26, 1.00)

  style.Colors[imgui.Col.FrameBgActive]          = imgui.ImVec4(0.25, 0.25, 0.26, 1.00)

  style.Colors[imgui.Col.TitleBg]                = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.TitleBgActive]          = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.TitleBgCollapsed]       = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.MenuBarBg]              = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.ScrollbarBg]            = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.ScrollbarGrab]          = imgui.ImVec4(0.00, 0.00, 0.00, 1.00)

  style.Colors[imgui.Col.ScrollbarGrabHovered]   = imgui.ImVec4(0.41, 0.41, 0.41, 1.00)

  style.Colors[imgui.Col.ScrollbarGrabActive]    = imgui.ImVec4(0.51, 0.51, 0.51, 1.00)

  style.Colors[imgui.Col.CheckMark]              = imgui.ImVec4(1.00, 1.00, 1.00, 1.00)

  style.Colors[imgui.Col.SliderGrab]             = imgui.ImVec4(0.21, 0.20, 0.20, 1.00)

  style.Colors[imgui.Col.SliderGrabActive]       = imgui.ImVec4(0.21, 0.20, 0.20, 1.00)

  style.Colors[imgui.Col.Button]                 = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.ButtonHovered]          = imgui.ImVec4(0.21, 0.20, 0.20, 1.00)

  style.Colors[imgui.Col.ButtonActive]           = imgui.ImVec4(0.41, 0.41, 0.41, 1.00)

  style.Colors[imgui.Col.Header]                 = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.HeaderHovered]          = imgui.ImVec4(0.20, 0.20, 0.20, 1.00)

  style.Colors[imgui.Col.HeaderActive]           = imgui.ImVec4(0.47, 0.47, 0.47, 1.00)

  style.Colors[imgui.Col.Separator]              = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.SeparatorHovered]       = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.SeparatorActive]        = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.ResizeGrip]             = imgui.ImVec4(1.00, 1.00, 1.00, 0.25)

  style.Colors[imgui.Col.ResizeGripHovered]      = imgui.ImVec4(1.00, 1.00, 1.00, 0.67)

  style.Colors[imgui.Col.ResizeGripActive]       = imgui.ImVec4(1.00, 1.00, 1.00, 0.95)

  style.Colors[imgui.Col.Tab]                    = imgui.ImVec4(0.12, 0.12, 0.12, 1.00)

  style.Colors[imgui.Col.TabHovered]             = imgui.ImVec4(0.28, 0.28, 0.28, 1.00)

  style.Colors[imgui.Col.TabActive]              = imgui.ImVec4(0.30, 0.30, 0.30, 1.00)

  style.Colors[imgui.Col.TabUnfocused]           = imgui.ImVec4(0.07, 0.10, 0.15, 0.97)

  style.Colors[imgui.Col.TabUnfocusedActive]     = imgui.ImVec4(0.14, 0.26, 0.42, 1.00)

  style.Colors[imgui.Col.PlotLines]              = imgui.ImVec4(0.61, 0.61, 0.61, 1.00)

  style.Colors[imgui.Col.PlotLinesHovered]       = imgui.ImVec4(1.00, 0.43, 0.35, 1.00)

  style.Colors[imgui.Col.PlotHistogram]          = imgui.ImVec4(0.90, 0.70, 0.00, 1.00)

  style.Colors[imgui.Col.PlotHistogramHovered]   = imgui.ImVec4(1.00, 0.60, 0.00, 1.00)

  style.Colors[imgui.Col.TextSelectedBg]         = imgui.ImVec4(1.00, 0.00, 0.00, 0.35)

  style.Colors[imgui.Col.DragDropTarget]         = imgui.ImVec4(1.00, 1.00, 0.00, 0.90)

  style.Colors[imgui.Col.NavHighlight]           = imgui.ImVec4(0.26, 0.59, 0.98, 1.00)

  style.Colors[imgui.Col.NavWindowingHighlight]  = imgui.ImVec4(1.00, 1.00, 1.00, 0.70)

  style.Colors[imgui.Col.NavWindowingDimBg]      = imgui.ImVec4(0.80, 0.80, 0.80, 0.20)

  style.Colors[imgui.Col.ModalWindowDimBg]       = imgui.ImVec4(0.00, 0.00, 0.00, 0.70)

end



imgui.OnInitialize(function()

  imgui.Theme()

end)


-- Menu

imgui.OnFrame(function() return window_state[0] end,

  function(player)





    

    imgui.SetNextWindowSize(imgui.ImVec2(imgui.GetFontSize() * 70, imgui.GetFontSize() * 50), imgui.Cond.FirstUseEver)

    imgui.Begin(u8'Помощник для Министерства Юстиции ' .. script.this.version, window_state)



    if imgui.BeginTabBar('Tabs') then

      if imgui.BeginTabItem(u8'Взаимодействие') then -- первая вкладка

local _, changed = imgui.InputInt(u8'Введите ID Игрока', aidi)

            if changed then

                    aidi = imgui.new.int(aidi[0])

                end





if imgui.Button(u8('pursuit                ')) then

                    lua_thread.create(function()





              sampSendChat('/pursuit ' .. aidi[0])





    end)

end



if imgui.Button(u8('/z                ')) then

                    lua_thread.create(function()





              sampSendChat('/z ' .. aidi[0])





    end)

end



  if imgui.Button(u8('Арест, вести за собой')) then

                    lua_thread.create(function()



            sampSendChat('/cuff ' .. aidi[0])



            wait(4000)

            sampSendChat('/gotome ' .. aidi[0])

                    end)

                end



 if imgui.Button(u8('Спиной на обыск')) then

                    lua_thread.create(function()



                        sampSendChat("Спиной ко мне, обыск")                      

                        

                    end)

                end         



        



if imgui.Button(u8('Обыск                   ')) then

                    lua_thread.create(function()



            sampSendChat('/frisk ' .. aidi[0])

                    end)

                end          







if imgui.Button(u8('В машину                ')) then



    lua_thread.create(function()



        sampSendChat('/incar ' .. aidi[0] .. ' 3')



    end)

end





if imgui.Button(u8('PULL                ')) then

                    lua_thread.create(function()



            sampSendChat('/pull ' .. aidi[0])

                    end)

                end          







        imgui.EndTabItem() -- конец вкладки

      end



if imgui.BeginTabItem(u8'Прочее') then -- вторая вкладка



                if imgui.Button(u8('Включить Камеру')) then

                    lua_thread.create(function()



                        sampSendChat("/do На груди висит скрытая боди камера.")

                        wait(1000)

                        sampSendChat("/me нажал на кнопку включения боди камеры")

                        wait(1000)

                        sampSendChat("/do Камера ведёт аудио и видео запись.")

                    wait(1000)

                    sampSendChat ("/time")





                    end)

                end



if imgui.Button(u8('Выключить Камеру')) then

                    lua_thread.create(function()



                        sampSendChat("/me нажал на кнопку выключения боди камеры")

                        wait(1000)

                        sampSendChat("/do Боди камера выключилась.")

    



                    end)

                end



if imgui.Button(u8('Миранда                 ')) then

                    lua_thread.create(function()

                        sampSendChat('Вы имеете право хранить молчание.')

wait(1000)

sampSendChat('Всё, что вы скажете, можете и будет использовано против Вас в суде.')

wait(1000)

sampSendChat('Ваш адвокат может присутствовать при допросе.')

wait(1000)

sampSendChat('Если Вы не можете оплатить услуги адвоката, он будет предоставлен Вам государством.')

wait(1000)

sampSendChat('Вы понимаете свои права?')

wait(1000)



    end)

end



if imgui.Button(u8('Причина                 ')) then

                    lua_thread.create(function()

                        sampSendChat('Вы арестованы за нахождение в розыске')





    end)

end



if imgui.Button(u8('КПЗ              ')) then

                    lua_thread.create(function()

                        sampSendChat('/me сорвал с груди рацию и попросил офицеров для сопровождения')

        wait(1000)

        sampSendChat('/me в ожидании офицеров приготовил бланк и вещи преступника')

        wait(1000)

        sampSendChat('/me заполняет описание нарушителя')

        wait(1000)

        sampSendChat('/do Из департамента выходят офицеры и забирают преступника с собой.')

        wait(1000)

        sampSendChat('/todo Смотри, мыло не роняй*улыбаясь, машет преступнику')

        

        wait(1000)

                        sampSendChat('/arrest ' .. aidi[0])

    end)

end



if imgui.Button(u8('Маску                   ')) then

                    lua_thread.create(function()



            sampSendChat('/unmask ' .. aidi[0]) 

                    end)

                end          







if imgui.Button(u8('Жетон                ')) then

                    lua_thread.create(function()

     sampSendChat('Федеральное Бюро Расследований!')

wait(1000)

                        sampSendChat('/me достал жетон с кармана')

        

               wait(1000)

sampSendChat('/do На жетоне надпись: FBI.')

        

               wait(1000)

sampSendChat('/do Ниже надпись: Агент.')

        

               wait(1000)

sampSendChat('/me показал жетон человеку')

        

               wait(1000)

sampSendChat('/me положил жетон в карман')

        

               wait(1000)



    end)

end



       





if imgui.Button(u8('Документики')) then

                    lua_thread.create(function()



                        sampSendChat("Предоставьте документ, удостоверяющий вашу личность, а именно паспорт.")                      

               wait(1000)

sampSendChat("/n /showpass id")         

                    end)

                end





        imgui.EndTabItem() -- конец вкладки

      end



if imgui.BeginTabItem(u8'WANTED') then -- вторая вкладка



                if imgui.Button(u8('WANTED 7')) then

                    lua_thread.create(function()



                        sampSendChat("/wanted 7")





                    end)

                end



if imgui.Button(u8('WANTED 6')) then

                    lua_thread.create(function()



                        sampSendChat("/wanted 6")



                        

                    end)

                end



if imgui.Button(u8('WANTED 5')) then

                    lua_thread.create(function()



                        sampSendChat("/wanted 5")



                        

                    end)

                end

if imgui.Button(u8('WANTED 4')) then

                    lua_thread.create(function()



                        sampSendChat("/wanted 4")



                        

                    end)

                end

if imgui.Button(u8('WANTED 3')) then

                    lua_thread.create(function()



                        sampSendChat("/wanted 3")



                        

                    end)

                end



if imgui.Button(u8('WANTED 2')) then

                    lua_thread.create(function()



                        sampSendChat("/wanted 2")



                        

                    end)

                end



if imgui.Button(u8('WANTED 1')) then

                    lua_thread.create(function()



                        sampSendChat("/wanted 1")



                        

                    end)

                end

           

 

if imgui.Button(u8('Вязка через /tie')) then

                    lua_thread.create(function()



                        

sampSendChat('/tie ' .. aidi[0])

                        wait(1000)

                        sampSendChat('/gotome ' .. aidi[0])

                        wait(1000)

                        sampSendChat("/todo Без резких движений*ведя преступника за собой")



    end)

end





        imgui.EndTabItem() -- конец вкладки

      end



if imgui.BeginTabItem(u8'Другое') then -- вторая вкладка



                if imgui.Button(u8('Для Пешехода')) then

                    lua_thread.create(function()



                        sampSendChat("/m Внимание! Оставайтесь на своем месте и не пытайтесь уйти")

                        wait(1000)

                        sampSendChat("/m В случае игнорирования требований будет открыт огонь!")



                   end)

                end



if imgui.Button(u8('Для Авто')) then

                    lua_thread.create(function()



                        sampSendChat("/m Водитель авто, остановитесь и заглушите двигатель, держите руки на руле!")

                        wait(1000)

                        sampSendChat("/m В случае игнорирования требований будет открыт огонь по авто!")





                    end)

                end





if imgui.Button(u8('Одеть/снять армор')) then

                    lua_thread.create(function()



                        sampSendChat("/armour")                      

                        

                    end)

                end



if imgui.Button(u8('Спец.средства')) then

                    lua_thread.create(function()



                        sampSendChat("Вы арестованы за ношение спец.средств. ")                      

                        

                    end)

                end

if imgui.Button(u8('Нарко')) then

                    lua_thread.create(function()



                        sampSendChat("Вы арестованы за хранение наркотиков")                      

                        

                    end)

                end



if imgui.Button(u8('repcar')) then

                    lua_thread.create(function()



                        sampSendChat("/repcar")                      

                        

                    end)

                end







if imgui.Button(u8('Изъятие')) then

                    lua_thread.create(function()





        sampSendChat('/me надел перчатки, достал зип-пакет, забрал улики у подозреваемого, положил их в зип-пакет ')



        wait(1000)

                       

                        sampSendChat('/take ' .. aidi[0])

                    end)

                end







if imgui.Button(u8('/time + /id                 ')) then

                    lua_thread.create(function()



            sampSendChat('/time')

            wait(1000)

            sampSendChat('/id ' .. aidi[0])

                    end)

                end             

        imgui.EndTabItem() -- конец вкладки

      end

      imgui.EndTabBar() -- конец всех вкладок

    end



    imgui.End()

  end

)













function main()

  sampRegisterChatCommand('mo', function() window_state[0] = not window_state[0] end)

  

  while true do

    wait(0)

  end

end

