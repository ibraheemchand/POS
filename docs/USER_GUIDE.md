# Nexora POS User Guide

This guide describes the delivered offline Windows application. The application stores data locally; FBR/network synchronization is not part of this release.

## 1. Install and start

1. Copy the complete `deploy` folder to the POS computer. Keep the executable together with its DLLs and subfolders.
2. Double-click `deploy\wholesale_pos.exe`.
3. On first start, Nexora creates the local SQLite database automatically.
4. If Windows reports a missing Qt DLL, do not copy only the executable. Recreate the deployment folder with `scripts\deploy.ps1` and Qt's `windeployqt`.

For this repository, the ready-to-run file is:

```text
E:\pos--2\deploy\wholesale_pos.exe
```

## 2. First-time setup

1. Open **Settings** from the left navigation.
2. Enter the business name, phone, currency, and receipt footer.
3. Set **Automatic backup interval** in hours, or leave it at `0` to disable scheduling. Restart after changing this value.
4. If using a raw thermal printer, enter its local device/shared path in **Thermal printer path**.
5. Select **Set or change security PIN** and save a 4–12 digit PIN. The PIN protects shift closing, supplier archiving, backup restore, and PIN removal.
6. Select **Save settings**.

All prices and amounts are entered in paisa. For example, `12500` means PKR 125.00.

## 3. Create inventory

1. Open **Inventory**.
2. Select **Add product**.
3. Enter the product name, base unit, and retail price in paisa.
4. Select **Edit selected** later to set purchase price, wholesale/dealer prices, minimum stock, barcode, or batch settings.
5. Use the search box to find a product by name, SKU, or barcode.
6. Select **Import CSV** for bulk loading. The first four columns must be:

   `name,base_unit,purchase_price_paisa,retail_price_paisa`

   Optional columns are SKU, barcode, wholesale price, dealer price, minimum stock, batch flag, expiry flag, description, and image path.

7. Use **Archive selected** to hide a product from active sales. Archiving does not delete historical transactions.
8. Configure a barcode and select **Print barcode label** when a thermal path is configured.

## 4. Receive a purchase

1. Open **Suppliers** and select **Add supplier** if the supplier does not exist.
2. Enter supplier name, contact, phone, address, and opening payable if applicable.
3. Open **Purchases**.
4. Select the supplier and product.
5. Enter quantity and cost in paisa.
6. For batch-tracked products, enter a batch number and expiry date.
7. Select **Add item** for each line.
8. Select **Receive purchase on credit**. Stock, purchase history, supplier payable, and cash/shift records are written transactionally.

## 5. Open and close a cash shift

1. Open **Cash Management**.
2. Select **Open shift** and enter opening cash in paisa.
3. Complete cash sales, cash purchases, and cash customer payments while the shift is open. The application attaches them to the active shift.
4. At closing, count the till.
5. Select **Close shift**, pass the security PIN, and enter counted cash.
6. Review expected cash and the difference shown by the reconciliation dialog.

## 6. Make a sale

1. Open **Sales POS**.
2. Search for a product or scan its barcode. Press Enter in the search field to add the first exact result.
3. Set quantity and select **Add to cart**.
4. Repeat for additional products.
5. Enter an optional invoice discount in paisa. It cannot exceed the subtotal.
6. Choose `cash` or `mixed` payment.
7. For mixed payment, enter cash, cheque, and mobile-wallet portions. The portions must equal the sale total.
8. Select **Complete sale**.
9. Use **Suspend sale** to save a cart locally and **Resume sale** to restore it later. Stock is rechecked when a suspended sale is resumed.

## 7. Customers and payments

1. Open **Customers** and select **Add customer**.
2. Enter name, phone, and credit limit in paisa.
3. To record a payment, select the customer and choose **Record payment**.
4. Select an outstanding invoice, enter the allocation amount, and choose the payment method.
5. The invoice balance, customer balance, customer ledger, and cash shift are updated together.

## 8. Suppliers and cheques

1. **Suppliers** lists active suppliers and payable balances.
2. Select a supplier and choose **View ledger** to see debit/credit history.
3. **Archive selected** requires the security PIN and removes the supplier from active purchasing.
4. Open **Cheques** to record received or issued cheques.
5. Select a cheque and choose **Update status** to mark it pending, deposited, cleared, or bounced.

Cheque reversal accounting is not enabled yet; bounced/cleared settlement rules require a business decision.

## 9. Returns and refunds

The transactional return services are implemented, but the current visible desktop shell does not yet expose dedicated return/refund buttons. Do not manually edit the database. Use the return service through an approved integration or wait for the dedicated Returns UI task to be delivered.

## 10. Reports and analytics

1. Open **Reports**.
2. Set the From and To dates.
3. Select **Run report**.
4. Use **Export CSV**, **Export PDF**, **Export Excel**, or **Print A4**.
5. Open **Analytics** for seven-day sales, purchases, receivables, low-stock metrics, and the daily sales trend.
6. Open **Audit Log** to filter sensitive actions by action name.

## 11. Backups and restore

1. Use **Backup now** on the Dashboard or open **Backup & Restore**.
2. Choose a local folder or USB drive.
3. Nexora writes a verified SQLite snapshot and `.sha256` checksum.
4. Keep both files together and copy them to separate media.
5. To restore, select a verified backup or choose **Restore from drive/file**.
6. Confirm the warning and pass the security PIN.
7. Nexora creates a safety backup of the current database before restoring.
8. Restart the application after a restore and verify the latest invoice, inventory, and opening cash.

## 12. Notifications and printing

- **Settings → View notifications** shows unread local notifications and marks displayed notifications as read.
- **Settings → Print test receipt** and **Print test barcode label** require a configured thermal device path.
- Thermal output is raw ESC/POS. A normal Windows desktop printer should use **Reports → Print A4** instead.

## 13. QA/demo data

The normal application never inserts demo data automatically.

```powershell
.\deploy\wholesale_pos.exe --seed-demo
```

For an isolated priced random fixture:

```powershell
.\deploy\wholesale_pos.exe `
  --data-dir=E:\qa-pos `
  --seed-random=25 `
  --seed=20260727
```

The same seed can be run repeatedly without duplicating the generated products.

## 14. Keyboard and safety tips

- Use Tab/Shift+Tab to move between controls and arrow keys in tables and navigation.
- Press Enter in the Sales POS search field to add the first matching product.
- Use Escape to cancel dialogs without saving.
- Verify the active shift before accepting cash.
- Keep the security PIN private.
- Back up before restores, bulk imports, or major catalog changes.
