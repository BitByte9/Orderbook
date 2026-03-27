const api = {
  async getBook() {
    const res = await fetch("/api/book");
    return res.json();
  },
  async add(payload) {
    const res = await fetch("/api/add", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    return res.json();
  },
  async cancel(payload) {
    const res = await fetch("/api/cancel", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    return res.json();
  },
  async modify(payload) {
    const res = await fetch("/api/modify", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    return res.json();
  },
};

function renderBook(book) {
  const bidsBody = document.getElementById("bids-body");
  const asksBody = document.getElementById("asks-body");
  bidsBody.innerHTML = "";
  asksBody.innerHTML = "";

  for (const lvl of book.bids || []) {
    const tr = document.createElement("tr");
    tr.innerHTML = `<td>${lvl.price}</td><td>${lvl.quantity}</td>`;
    bidsBody.appendChild(tr);
  }
  for (const lvl of book.asks || []) {
    const tr = document.createElement("tr");
    tr.innerHTML = `<td>${lvl.price}</td><td>${lvl.quantity}</td>`;
    asksBody.appendChild(tr);
  }
}

function renderTrades(trades) {
  const log = document.getElementById("trades-log");
  if (!trades || trades.length === 0) {
    log.textContent = "No trades.";
    return;
  }
  log.textContent = trades
    .map(
      (t) =>
        `bidId=${t.bidId}, askId=${t.askId}, qty=${t.quantity}, bidPx=${t.bidPrice}, askPx=${t.askPrice}`
    )
    .join("\n");
}

async function refreshBook() {
  const data = await api.getBook();
  renderBook(data.book);
}

function setAddStatus(message, isError = false) {
  const el = document.getElementById("add-status");
  el.textContent = message;
  el.style.color = isError ? "#fca5a5" : "#93c5fd";
}

const addTypeEl = document.getElementById("add-type");
const addPriceEl = document.getElementById("add-price");
const addFormEl = document.getElementById("add-form");

function syncAddPriceMode() {
  const isMarket = addTypeEl.value === "mkt";
  if (isMarket) {
    addPriceEl.value = "0";
    addPriceEl.disabled = true;
  } else {
    addPriceEl.disabled = false;
  }
}

document.getElementById("add-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const submitBtn = addFormEl.querySelector("button[type='submit']");
  submitBtn.disabled = true;
  setAddStatus("Adding order...");
  const payload = {
    id: Number(document.getElementById("add-id").value),
    side: document.getElementById("add-side").value,
    qty: Number(document.getElementById("add-qty").value),
    type: document.getElementById("add-type").value,
    price: Number(document.getElementById("add-price").value),
  };

  if (!Number.isFinite(payload.id) || payload.id <= 0) {
    setAddStatus("Order ID must be a positive number.", true);
    submitBtn.disabled = false;
    return;
  }
  if (!Number.isFinite(payload.qty) || payload.qty <= 0) {
    setAddStatus("Quantity must be a positive number.", true);
    submitBtn.disabled = false;
    return;
  }
  if (payload.type !== "mkt" && (!Number.isFinite(payload.price) || payload.price <= 0)) {
    setAddStatus("Price must be a positive number for non-market orders.", true);
    submitBtn.disabled = false;
    return;
  }

  try {
    const data = await api.add(payload);
    if (!data.ok) {
      setAddStatus(data.error || "Add failed.", true);
      return;
    }
    renderBook(data.book);
    renderTrades(data.trades);
    setAddStatus(`Order ${payload.id} submitted.`);
  } catch (err) {
    setAddStatus(`Request failed: ${err.message}`, true);
  } finally {
    submitBtn.disabled = false;
  }
});

document.getElementById("modify-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const payload = {
    id: Number(document.getElementById("mod-id").value),
    side: document.getElementById("mod-side").value,
    qty: Number(document.getElementById("mod-qty").value),
    price: Number(document.getElementById("mod-price").value),
  };
  const data = await api.modify(payload);
  renderBook(data.book);
  renderTrades(data.trades);
});

document.getElementById("cancel-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const payload = {
    id: Number(document.getElementById("cancel-id").value),
  };
  const data = await api.cancel(payload);
  renderBook(data.book);
  renderTrades(data.trades);
});

document.getElementById("refresh-btn").addEventListener("click", refreshBook);
addTypeEl.addEventListener("change", syncAddPriceMode);
syncAddPriceMode();

refreshBook().catch((err) => {
  const log = document.getElementById("trades-log");
  log.textContent = `Failed to load book: ${err.message}`;
});
