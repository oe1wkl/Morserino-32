-- normalize_ids.lua
-- Normalizes heading IDs for PDF link compatibility.
-- Pandoc derives heading IDs directly from heading text, so German
-- headings produce IDs containing umlauts (e.g. "anschlüsse-...").
-- Weasyprint cannot reliably resolve such Unicode fragment identifiers
-- as internal PDF links. This filter replaces them with ASCII
-- equivalents before HTML generation, so TOC hrefs and heading IDs
-- always match and weasyprint can create working PDF links.
--
-- Because filters run on the AST before output is rendered, pandoc
-- regenerates the TOC from the modified identifiers automatically.

local replacements = {
    -- lower case
    ['\195\164'] = 'ae',   -- ä  (UTF-8: C3 A4)
    ['\195\182'] = 'oe',   -- ö  (UTF-8: C3 B6)
    ['\195\188'] = 'ue',   -- ü  (UTF-8: C3 BC)
    ['\195\159'] = 'ss',   -- ß  (UTF-8: C3 9F)
    -- upper case (pandoc lowercases IDs, but just in case)
    ['\195\132'] = 'ae',   -- Ä  (UTF-8: C3 84)
    ['\195\150'] = 'oe',   -- Ö  (UTF-8: C3 96)
    ['\195\156'] = 'ue',   -- Ü  (UTF-8: C3 9C)
}

function Header(h)
    local id = h.identifier
    for bytes, ascii in pairs(replacements) do
        id = id:gsub(bytes, ascii)
    end
    h.identifier = id
    return h
end
