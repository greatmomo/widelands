-- =======================================================================
--                 Texts for the Atlantean tutorial mission
-- =======================================================================

-- =========================
-- Some formating functions
-- =========================
-- Rich Text
function rt(text_or_opts, text)
   k = "<rt>"
   if text then
      k = ("<rt %s>"):format(text_or_opts)
   else
      text = text_or_opts
   end

   return k .. text .. "</rt>"
end

-- Headings
function h1(s)
   return "<p font=FreeSerif font-size=18 font-weight=bold font-color=D1D1D1>"
      ..  s .. "<br></p><p font-size=8> <br></p>"
end

function h2(s)
   return "<p font=FreeSerif font-size=12 font-weight=bold font-color=D1D1D1>"
      ..  s .. "<br></p><p font-size=4> <br></p>"
end

-- Simple flowing text. One Paragraph
function p(s)
   return "<p line-spacing=3 font-size=12>" .. s .. "<br></p>" ..
      "<p font-size=8> <br></p>"
end

-- Direct speech by one of the persons that appear in the map
function speech(img, clr, g_title, g_text)
   local title, text = g_title, g_text
   if not text then
      title = nil
      text = g_title
   end
     
   local s = ""
   if title then
      s = rt("<p font-size=20 font-weight=bold font-face=FreeSerif " .. 
         ("font-color=%s>"):format(clr) .. title ..
         "</p><p font-size=8> <br></p>"
      )
   end

   return s .. rt(("image=%s"):format(img), p(text))
end

function princess(title, text)
   return speech("map:princess.png", "2F9131", title, text)
end

-- Nice formatting for objective texts: A header and one paragraph
-- of text
function obj_text(heading, body)
   return rt(h2(heading) .. p(body))
end

-- Append an objective text to a dialog box in a nice fashion.
function new_objectives(...) 
   local s = rt(
   "<p font=FreeSerif font-size=18 font-weight=bold font-color=D1D1D1>"
   .. _"New Objectives" .. "</p>")

   for idx,obj in ipairs{...} do
      s = s .. obj.body
   end
   return s
end

-- =======================================================================
--                                Objectives                                
-- =======================================================================
obj_be_a_smarty = {
   name = "just_a_unique_name",
   title = _ "Summary of what to do",
   body = obj_text(_"This is the heading!", _
[[Just be cool and expand west, buddy!]]
   ),
}


-- =======================================================================
--                                  Texts                                  
-- =======================================================================

-- This is an array or message descriptions
initial_messages = {
-- Message 01
{
   title = _ "Though speaked: It shall be light",
   body = rt(
      h1(_"This is a caption. And it is translated") ..
      p(_
[[This is one paragraph. It is also translated and it spans
multiple lines. Every newline is converted to a space, which
is what we usually want. To keep it readable, we reformat the paragraphs
to be < 80 chars in the Lua source code. SirVer can do this, do not
bother too much about it.]]
      ) .. p(
[[This is one more paragraph. It is *not* translated, because the _ is missing
in front.  In other words a string like this _ "Hi" is translated, one without
_ is not. Capice?]]
      )
   ),
},
-- Message 02, direct speech now
{
   title = _ "And now I speak!",
   body = princess(_ "The princess speaks", _
[[Let's sink this ship!]]
   ) .. new_objectives(obj_be_a_smarty)
}
} -- end of initial messages.

-- Add similar dialog queues. To test them use a debug build, press F6 and
-- enter msg_boxes(initial_messages) or similar.
