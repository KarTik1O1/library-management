const $ = (sel) => document.querySelector(sel);
const $$ = (sel) => document.querySelectorAll(sel);

const FINE_PER_DAY = 5;     // must match FINE_PER_DAY in library.h

let isAdmin = false;
let allBooks = [];
let members = [];
let statusFilter = "all";   // all | available | onloan | overdue
let locationFilter = "";    // "" means every shelf

// ---------- Helpers ----------
const get = (path) => fetch(path).then((r) => r.json());

async function post(path, data = {}) {
  const res = await fetch(path, { method: "POST", body: new URLSearchParams(data) });
  const json = await res.json();
  if (!res.ok) throw new Error(json.message || "Something went wrong");
  return json;
}

function toast(msg, isError = false) {
  const t = $("#toast");
  t.textContent = msg;
  t.className = "toast show" + (isError ? " error" : "");
  clearTimeout(t._timer);
  t._timer = setTimeout(() => (t.className = "toast"), 2800);
}

const esc = (s) =>
  String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

const fmtDate = (sec) =>
  new Date(sec * 1000).toLocaleDateString(undefined, { day: "numeric", month: "short", year: "numeric" });

const pad = (n) => String(n).padStart(2, "0");

// Turns milliseconds into "2d 05h 13m 09s" or "05:13:09"
function fmtRemain(ms) {
  const s = Math.floor(ms / 1000);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  return d > 0 ? `${d}d ${pad(h)}h ${pad(m)}m ${pad(sec)}s` : `${pad(h)}:${pad(m)}:${pad(sec)}`;
}

// ---------- Theme ----------
function applyTheme(light) {
  document.body.classList.toggle("light", light);
  localStorage.setItem("theme", light ? "light" : "dark");
  $("#themeBtn").textContent = light ? "🌙 Dark" : "☀️ Light";
}

// ---------- Tabs and roles ----------
function showTab(name) {
  $$(".tab").forEach((b) => b.classList.toggle("active", b.dataset.tab === name));
  $$(".section").forEach((s) => s.classList.toggle("active", s.id === name));
  window.scrollTo({ top: 0, behavior: "smooth" });
}

function applyRole() {
  $$(".admin-only").forEach((el) => el.classList.toggle("hidden", !isAdmin));
  $("#loginBtn").classList.toggle("hidden", isAdmin);
  $("#logoutBtn").classList.toggle("hidden", !isAdmin);
  $("#roleBadge").textContent = isAdmin ? "Admin" : "Guest (view only)";

  const active = document.querySelector(".section.active");
  if (!isAdmin && active && active.classList.contains("admin-only")) showTab("dashboard");
}

// ---------- Loading data ----------
async function loadAll() {
  allBooks = await get("/api/books");
  members = isAdmin ? await get("/api/members") : [];
  renderStats();
  renderShelves();
  renderLocationOptions();
  renderBooks();
  renderMembers();
  renderCirculation();
  renderLoans();
}

// ---------- Filtering ----------
function filteredBooks() {
  const q = $("#search").value.trim().toLowerCase();
  return allBooks.filter((b) => {
    if (q && ![b.title, b.author, b.isbn, b.location].some((f) => f.toLowerCase().includes(q))) return false;
    if (locationFilter && b.location !== locationFilter) return false;
    if (statusFilter === "available" && !b.available) return false;
    if (statusFilter === "onloan" && b.available) return false;
    if (statusFilter === "overdue" && !b.overdue) return false;
    return true;
  });
}

function syncChips() {
  $$(".chip").forEach((c) => c.classList.toggle("active", c.dataset.filter === statusFilter));
}

function goToBooks(status, loc) {
  statusFilter = status;
  locationFilter = loc;
  $("#search").value = "";
  renderLocationOptions();
  syncChips();
  showTab("books");
  renderBooks();
}

// ---------- Rendering ----------
function renderStats() {
  const total = allBooks.length;
  const avail = allBooks.filter((b) => b.available).length;
  $("#s-total").textContent = total;
  $("#s-avail").textContent = avail;
  $("#s-loan").textContent = total - avail;
  $("#s-over").textContent = allBooks.filter((b) => b.overdue).length;
}

function locations() {
  return [...new Set(allBooks.map((b) => b.location))].sort();
}

function renderShelves() {
  const counts = {};
  allBooks.forEach((b) => (counts[b.location] = (counts[b.location] || 0) + 1));
  const keys = Object.keys(counts).sort();
  $("#shelfList").innerHTML = keys.length
    ? keys
        .map((l) => `<button class="shelf" data-loc="${esc(l)}">
            <span>${esc(l)}</span><b>${counts[l]} book${counts[l] > 1 ? "s" : ""}</b>
          </button>`)
        .join("")
    : `<p class="muted">No books yet.</p>`;
}

function renderLocationOptions() {
  const sel = $("#locationFilter");
  sel.innerHTML =
    `<option value="">All locations</option>` +
    locations().map((l) => `<option value="${esc(l)}">${esc(l)}</option>`).join("");
  sel.value = locationFilter;
  $("#locList").innerHTML = locations().map((l) => `<option value="${esc(l)}">`).join("");
}

function renderBooks() {
  const list = filteredBooks();
  $("#bookCount").textContent = `(${list.length} of ${allBooks.length})`;
  const tbody = $("#bookRows");

  if (!list.length) {
    tbody.innerHTML = `<tr><td colspan="6" class="empty">No books match these filters</td></tr>`;
    return;
  }

  tbody.innerHTML = list
    .map((b) => {
      let status;
      if (b.available) {
        status = `<span class="badge b-ok">Available</span>`;
      } else {
        status = `<span class="badge ${b.overdue ? "b-late" : "b-out"}">${b.overdue ? "Overdue" : "On loan"}</span>`;
        if (isAdmin) status += `<div class="muted small">${esc(b.holder)} · due ${fmtDate(b.due)}</div>`;
      }

      let actions = "";
      if (isAdmin) {
        actions += `<button class="btn ghost small" data-move="${b.id}" data-current="${esc(b.location)}" data-title="${esc(b.title)}">Move</button>`;
        if (b.available)
          actions += ` <button class="btn danger small" data-del="${b.id}">Delete</button>`;
      }

      return `<tr>
        <td>${b.id}</td>
        <td><strong>${esc(b.title)}</strong></td>
        <td>${esc(b.author)}</td>
        <td><span class="loc">${esc(b.location)}</span></td>
        <td>${status}</td>
        <td class="row-actions">${actions}</td>
      </tr>`;
    })
    .join("");
}

function renderMembers() {
  if (!isAdmin) return;
  $("#memberRows").innerHTML = members.length
    ? members
        .map((m) => `<tr>
          <td>${m.id}</td>
          <td>${esc(m.name)}</td>
          <td>${m.type}</td>
          <td>${m.issued} / ${m.max}</td>
          <td class="row-actions">
            <button class="btn ghost small" data-edit-member="${m.id}" data-name="${esc(m.name)}">Edit name</button>
            <button class="btn danger small" data-del-member="${m.id}" data-name="${esc(m.name)}">Delete</button>
          </td>
        </tr>`)
        .join("")
    : `<tr><td colspan="5" class="empty">No members yet</td></tr>`;
}

function fillSelect(sel, list, label, emptyText) {
  sel.innerHTML = list.length
    ? list.map((x) => `<option value="${x.id}">${esc(label(x))}</option>`).join("")
    : `<option value="">${emptyText}</option>`;
}

function renderCirculation() {
  if (!isAdmin) return;
  const available = allBooks.filter((b) => b.available);
  const onLoan = allBooks.filter((b) => !b.available);

  fillSelect($("#issueBook"), available, (b) => `#${b.id} ${b.title} (${b.location})`, "No available books");
  fillSelect($("#issueMember"), members, (m) => `${m.name} (${m.type}, ${m.issued}/${m.max})`, "No members");
  fillSelect($("#returnBook"), onLoan, (b) => `#${b.id} ${b.title} (${b.holder})`, "No books on loan");
}

// Loans table with a live countdown and charges for overdue books
function renderLoans() {
  if (!isAdmin) return;
  const loans = allBooks.filter((b) => !b.available);

  $("#loanList").innerHTML = loans.length
    ? `<div class="table-wrap"><table>
        <thead><tr><th>Book</th><th>Member</th><th>Time left / overdue</th><th>Charges to pay</th></tr></thead>
        <tbody>
          ${loans
            .map((b) => `<tr>
              <td><strong>${esc(b.title)}</strong></td>
              <td>${esc(b.holder)}</td>
              <td class="timer" data-due="${b.due}"></td>
              <td class="charge" data-due="${b.due}"></td>
            </tr>`)
            .join("")}
        </tbody>
      </table></div>`
    : `<p class="muted">No books are on loan.</p>`;

  tickLoans();
}

// Runs every second so the countdown moves without reloading
function tickLoans() {
  const now = Date.now();

  document.querySelectorAll(".timer").forEach((el) => {
    const diff = Number(el.dataset.due) * 1000 - now;
    el.innerHTML =
      diff >= 0
        ? `<span class="badge b-ok">${fmtRemain(diff)} left</span>`
        : `<span class="badge b-late">Overdue by ${fmtRemain(-diff)}</span>`;
  });

  document.querySelectorAll(".charge").forEach((el) => {
    const lateSec = (now - Number(el.dataset.due) * 1000) / 1000;
    if (lateSec <= 0) {
      el.innerHTML = `<span class="muted">None</span>`;
      return;
    }
    const days = Math.ceil(lateSec / 86400);     // same rule as the C++ returnBook()
    el.innerHTML =
      `<strong class="due-fine">Rs. ${days * FINE_PER_DAY}</strong> ` +
      `<span class="muted">(${days} late day${days > 1 ? "s" : ""}, collect on return)</span>`;
  });
}

// ---------- Events ----------
document.addEventListener("click", async (e) => {
  const el = e.target.closest("button");
  if (!el) return;

  if (el.dataset.tab) return showTab(el.dataset.tab);
  if (el.dataset.stat) return goToBooks(el.dataset.stat, "");
  if (el.classList.contains("shelf")) return goToBooks("all", el.dataset.loc);
  if (el.dataset.filter) {
    statusFilter = el.dataset.filter;
    syncChips();
    return renderBooks();
  }

  if (el.dataset.del) {
    if (!confirm("Delete this book?")) return;
    try {
      await post("/api/books/delete", { id: el.dataset.del });
      toast("Book deleted");
      await loadAll();
    } catch (err) {
      toast(err.message, true);
    }
    return;
  }

  if (el.dataset.move) {
    const loc = prompt(`New shelf location for "${el.dataset.title}":`, el.dataset.current);
    if (loc === null || !loc.trim()) return;
    try {
      await post("/api/books/location", { id: el.dataset.move, location: loc.trim() });
      toast("Location updated");
      await loadAll();
    } catch (err) {
      toast(err.message, true);
    }
    return;
  }

  if (el.dataset.editMember) {
    const name = prompt("New name for this member:", el.dataset.name);
    if (name === null || !name.trim()) return;
    try {
      await post("/api/members/update", { id: el.dataset.editMember, name: name.trim() });
      toast("Member name updated");
      await loadAll();
    } catch (err) {
      toast(err.message, true);
    }
    return;
  }

  if (el.dataset.delMember) {
    if (!confirm(`Delete member "${el.dataset.name}"? This is blocked if they still have books.`)) return;
    try {
      await post("/api/members/delete", { id: el.dataset.delMember });
      toast("Member deleted");
      await loadAll();
    } catch (err) {
      toast(err.message, true);
    }
  }
});

$("#search").addEventListener("input", renderBooks);

$("#locationFilter").addEventListener("change", (e) => {
  locationFilter = e.target.value;
  renderBooks();
});

$("#clearBtn").addEventListener("click", () => {
  statusFilter = "all";
  locationFilter = "";
  $("#search").value = "";
  renderLocationOptions();
  syncChips();
  renderBooks();
  toast("Filters cleared");
});

$("#themeBtn").addEventListener("click", () => {
  applyTheme(!document.body.classList.contains("light"));
});

$("#sampleBtn").addEventListener("click", async () => {
  try {
    const r = await post("/api/books/sample");
    toast(r.message);
    await loadAll();
  } catch (err) {
    toast(err.message, true);
  }
});

$("#addBookForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    await post("/api/books", Object.fromEntries(new FormData(e.target)));
    e.target.reset();
    toast("Book added");
    await loadAll();
  } catch (err) {
    toast(err.message, true);
  }
});

$("#addMemberForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    await post("/api/members", Object.fromEntries(new FormData(e.target)));
    e.target.reset();
    toast("Member added");
    await loadAll();
  } catch (err) {
    toast(err.message, true);
  }
});

$("#issueForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    const r = await post("/api/issue", {
      bookId: $("#issueBook").value,
      memberId: $("#issueMember").value,
    });
    toast(r.message);
    await loadAll();
  } catch (err) {
    toast(err.message, true);
  }
});

$("#returnForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    const r = await post("/api/return", { bookId: $("#returnBook").value });
    toast(r.message);
    await loadAll();
  } catch (err) {
    toast(err.message, true);
  }
});

$("#loginForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    await post("/api/login", { password: new FormData(e.target).get("password") });
    e.target.reset();
    isAdmin = true;
    applyRole();
    await loadAll();
    showTab("books");
    toast("Logged in as admin");
  } catch (err) {
    toast(err.message, true);
  }
});

$("#logoutBtn").addEventListener("click", async () => {
  await post("/api/logout").catch(() => {});
  isAdmin = false;
  applyRole();
  statusFilter = "all";
  locationFilter = "";
  showTab("dashboard");
  await loadAll();
  toast("Logged out");
});

// ---------- Start ----------
async function init() {
  applyTheme(localStorage.getItem("theme") === "light");
  const me = await get("/api/me");
  isAdmin = me.admin;
  applyRole();
  await loadAll();
  setInterval(tickLoans, 1000);
}
init().catch(() => toast("Could not reach the server", true));