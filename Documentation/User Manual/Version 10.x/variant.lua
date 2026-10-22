--[[
variant.lua -- build one hardware variant of the manual from the tagged source.

  pandoc manual_en.md --lua-filter=variant.lua -M variant=pocket ...

With no `variant` metadata the filter does nothing at all, so the combined
manual builds exactly as before.

Vocabulary (see devdocs/manual-variants/README.md):

  classic       Morserino-32 1st and 2nd edition (OLED)
  pocket        Morserino-32 Pocket (TFT)
  pocket-a11y   Pocket accessibility edition

Untagged content belongs to every variant. A tag lists the variants that keep
the content, so `{.pocket .pocket-a11y}` survives in both Pocket builds and
`{.classic .pocket}` survives everywhere except the accessibility edition.
`pocket-a11y` is NOT implied by `pocket`.

What it strips
--------------
  fenced divs   `::: {.classic}` … `:::`         whole block removed
  spans         `[text]{.classic}`                inline run removed
  table rows    `| []{.classic}RSSI | … |`        whole row removed
                (an empty span at the very start of a row's first cell marks
                 the row; a span anywhere else is an ordinary inline tag)
  list items    an item whose entire text was one stripped span is dropped,
                rather than left as an empty bullet

Content that survives has its variant classes removed, and an element left with
no attributes at all is unwrapped -- so a kept `::: {.pocket}` around a section
leaves exactly the markup an untagged source would have produced, and the
variant build is not subtly different from the combined one.

What it reports
---------------
Cross-references in this manual are hand-typed section numbers, and filtering
renumbers the document -- so a reference can silently come to point at the wrong
section, or at one that is no longer there. The filter cannot fix that, but it
counts internal links whose target no longer exists and warns, so the failure is
loud instead of silent.
]]

local variants = { classic = true, pocket = true, ["pocket-a11y"] = true }

local target = nil          -- nil = no filtering
local removed = { div = 0, span = 0, row = 0, item = 0 }


local function variant_classes(attr)
  local found = {}
  for _, c in ipairs(attr.classes) do
    if variants[c] then found[#found + 1] = c end
  end
  return found
end

local function wanted(vc)
  for _, c in ipairs(vc) do
    if c == target then return true end
  end
  return false
end

--- Drop the variant classes; unwrap when nothing else is left.
local function unwrap_or_strip(el)
  local kept = {}
  for _, c in ipairs(el.attr.classes) do
    if not variants[c] then kept[#kept + 1] = c end
  end
  el.attr.classes = kept
  -- attr.attributes is an AttributeList, not a plain table: count it by walking.
  local n = 0
  for _ in pairs(el.attr.attributes) do n = n + 1 end
  if #kept == 0 and el.attr.identifier == "" and n == 0 then
    return el.content
  end
  return el
end


-- ── pass 1: metadata ────────────────────────────────────────────────────────
local read_meta = {
  Meta = function(meta)
    if meta.variant == nil then return nil end
    target = pandoc.utils.stringify(meta.variant)
    if target == "" then
      target = nil
    elseif not variants[target] then
      error(string.format(
        "variant.lua: unknown variant '%s' -- expected one of classic, "
        .. "pocket, pocket-a11y", target))
    end
    return nil
  end
}


-- ── pass 2: table rows ──────────────────────────────────────────────────────
-- Runs before spans are stripped, or the row markers would be gone by the time
-- the rows are inspected.

--- The empty variant span at the start of a row, if the row carries one.
local function row_marker(row)
  local cell = row.cells[1]
  if not cell or #cell.contents == 0 then return nil end
  local block = cell.contents[1]
  if not block or not block.content then return nil end
  local first = block.content[1]
  if first and first.t == "Span" and #first.content == 0 then
    local vc = variant_classes(first.attr)
    if #vc > 0 then return vc end
  end
  return nil
end

local function filter_rows(rows)
  local kept = {}
  for _, row in ipairs(rows) do
    local vc = row_marker(row)
    if vc == nil then
      kept[#kept + 1] = row
    elseif wanted(vc) then
      -- keep the row, drop the marker so it cannot reach the output
      local block = row.cells[1].contents[1]
      local inlines = {}
      for i = 2, #block.content do inlines[#inlines + 1] = block.content[i] end
      block.content = inlines
      row.cells[1].contents[1] = block
      kept[#kept + 1] = row
    else
      removed.row = removed.row + 1
    end
  end
  return kept
end

local strip_rows = {
  Table = function(tbl)
    if target == nil then return nil end
    for _, body in ipairs(tbl.bodies) do
      body.body = filter_rows(body.body)
    end
    return tbl
  end
}


-- ── pass 3: divs and spans ──────────────────────────────────────────────────
local strip_variants = {
  Div = function(el)
    if target == nil then return nil end
    local vc = variant_classes(el.attr)
    if #vc == 0 then return nil end
    if not wanted(vc) then
      removed.div = removed.div + 1
      return {}
    end
    return unwrap_or_strip(el)
  end,

  Span = function(el)
    if target == nil then return nil end
    local vc = variant_classes(el.attr)
    if #vc == 0 then return nil end
    if not wanted(vc) then
      removed.span = removed.span + 1
      return {}
    end
    return unwrap_or_strip(el)
  end
}


-- ── pass 4: list items emptied by a stripped span ───────────────────────────
local function is_blank(item)
  for _, block in ipairs(item) do
    if block.t ~= "Para" and block.t ~= "Plain" then return false end
    if pandoc.utils.stringify(block):match("%S") then return false end
  end
  return true
end

local function drop_blank_items(items)
  local kept = {}
  for _, item in ipairs(items) do
    if is_blank(item) then
      removed.item = removed.item + 1
    else
      kept[#kept + 1] = item
    end
  end
  return kept
end

local prune_lists = {
  BulletList = function(el)
    if target == nil then return nil end
    el.content = drop_blank_items(el.content)
    if #el.content == 0 then return {} end
    return el
  end,

  OrderedList = function(el)
    if target == nil then return nil end
    el.content = drop_blank_items(el.content)
    if #el.content == 0 then return {} end
    return el
  end
}


-- ── pass 5: report ──────────────────────────────────────────────────────────

--- Section numbers as --number-sections will assign them AFTER filtering.
-- Cross-references in this manual are hand-typed ("see section **5.8.4
-- Uploading a Text File**"). Removing a section renumbers everything after it,
-- so a reference that is right in the combined manual can be wrong here. Only
-- the filter knows what was removed, so only the filter can check this.
local function section_numbers(doc)
  local counters, numbers = {}, {}
  doc:walk({
    Header = function(h)
      for _, c in ipairs(h.classes) do
        if c == "unnumbered" then return nil end
      end
      counters[h.level] = (counters[h.level] or 0) + 1
      for lvl = h.level + 1, 6 do counters[lvl] = nil end
      local parts = {}
      for lvl = 1, h.level do parts[#parts + 1] = tostring(counters[lvl] or 0) end
      numbers[table.concat(parts, ".")] =
        pandoc.utils.stringify(h.content):gsub("%s+", " "):gsub("^%s*(.-)%s*$", "%1")
      return nil
    end
  })
  return numbers
end

local function check_crossrefs(doc)
  local numbers = section_numbers(doc)
  local bad = {}
  doc:walk({
    Strong = function(st)
      local text = pandoc.utils.stringify(st):gsub("%s+", " ")
      local num, title = text:match("^(%d+%.[%d%.]*%d)%s+(.+)$")
      if not num then return nil end
      local actual = numbers[num]
      local want = title:gsub("%s+$", ""):gsub("%.$", "")
      if actual == nil then
        bad[#bad + 1] = string.format("%s %s -> no section %s in this variant",
                                      num, want, num)
      elseif actual:lower() ~= want:lower() then
        bad[#bad + 1] = string.format("%s %s -> %s is now \"%s\"",
                                      num, want, num, actual)
      end
      return nil
    end
  })
  return bad
end

local report = {
  Pandoc = function(doc)
    if target == nil then return nil end

    local ids = {}
    doc:walk({
      Header = function(h) if h.identifier ~= "" then ids[h.identifier] = true end end,
      Div    = function(d) if d.identifier ~= "" then ids[d.identifier] = true end end,
      Span   = function(s) if s.identifier ~= "" then ids[s.identifier] = true end end
    })

    local dangling = {}
    doc:walk({
      Link = function(l)
        local anchor = l.target:match("^#(.+)$")
        if anchor and not ids[anchor] then
          dangling[#dangling + 1] = anchor
        end
      end
    })

    io.stderr:write(string.format(
      "variant.lua [%s]: removed %d div(s), %d span(s), %d table row(s), "
      .. "%d list item(s)\n",
      target, removed.div, removed.span, removed.row, removed.item))

    if #dangling > 0 then
      io.stderr:write(string.format(
        "variant.lua [%s]: WARNING - %d internal link(s) point at a section "
        .. "that is not in this variant:\n", target, #dangling))
      for _, a in ipairs(dangling) do
        io.stderr:write("    #" .. a .. "\n")
      end
    end

    local bad = check_crossrefs(doc)
    if #bad > 0 then
      io.stderr:write(string.format(
        "variant.lua [%s]: WARNING - %d hand-typed cross-reference(s) no "
        .. "longer match this variant's numbering:\n", target, #bad))
      for _, b in ipairs(bad) do
        io.stderr:write("    " .. b .. "\n")
      end
    end
    return nil
  end
}


return { read_meta, strip_rows, strip_variants, prune_lists, report }
