#pragma once

#include <string>
#include <vector>

/// Master-data CSV import: the canonical field keys shared by the ppSetup
/// mapping UI and the ppServer importer.
///
/// ppSetup writes the user's column mapping into a JSON spec using these keys
/// and ppServer reads the same keys back. A mismatch would silently drop a
/// column, so the catalogue lives in one header compiled into both binaries
/// (keep it dependency-free: STL only, no wx, no SQLite).
namespace MasterData {

enum class Kind { Stock, Member, Customer, Supplier };

struct FieldDef {
    const char* key;    // JSON key used by the import spec
    const char* label;  // shown in the mapping dialog
    bool required = false;
    bool numeric = false;
};

inline const char* KindCode(Kind kind) {
    switch (kind) {
        case Kind::Stock: return "stock";
        case Kind::Member: return "member";
        case Kind::Customer: return "customer";
        case Kind::Supplier: return "supplier";
    }
    return "stock";
}

inline const char* KindLabel(Kind kind) {
    switch (kind) {
        case Kind::Stock: return "Stock items (incl. stock types and opening quantity)";
        case Kind::Member: return "Membership";
        case Kind::Customer: return "Customers";
        case Kind::Supplier: return "Suppliers";
    }
    return "Stock items";
}

inline bool KindFromCode(const std::string& code, Kind& kind) {
    if (code == "stock") { kind = Kind::Stock; return true; }
    if (code == "member") { kind = Kind::Member; return true; }
    if (code == "customer") { kind = Kind::Customer; return true; }
    if (code == "supplier") { kind = Kind::Supplier; return true; }
    return false;
}

/// The order of the entries is the order the mapping dialog lists them, so the
/// compulsory identity fields stay at the top of each group.
inline const std::vector<FieldDef>& Fields(Kind kind) {
    static const std::vector<FieldDef> stock = {
        {"code", "Stock code", true, false},
        {"name", "Description / name", true, false},
        {"regNo", "Registration No (MAL)", false, false},
        {"type", "Stock type (normalised into Types)", false, false},
        {"packType", "Packaging type (BOX, STRIP, BOTTLE...)", false, false},
        {"poisonCategory", "Poison category (Drug Schedule A / B / C)", false, false},
        {"isPoison", "Poison item (yes / no)", false, false},
        {"category", "Stock category", false, false},
        {"packSize", "Units per pack", false, true},
        {"packPrice", "Selling price per pack", false, true},
        {"loosePrice", "Selling price per loose unit", false, true},
        {"looseCost", "Cost per loose unit", false, true},
        {"quantity", "Opening quantity (packs)", false, true},
        {"membershipPrice", "Membership price (per pack)", false, true},
        {"maxDiscount", "Max discount %", false, true},
        {"reorderLevel", "Reorder level", false, true},
        {"reorderQty", "Reorder quantity", false, true},
        {"barcode", "Barcode", false, false},
        {"brand", "Brand", false, false},
        {"genericName", "Generic / chemical name", false, false},
        {"strength", "Strength", false, false},
        // Shelf / location code. The client keeps shelves as entities linked to
        // the item (Stock Edit -> Shelf tab), so this maps to that list.
        {"shelfNo", "Shelf / location code", false, false},
        {"remark", "Remark", false, false},
    };

    static const std::vector<FieldDef> member = {
        {"code", "Member code", false, false},
        {"name", "Name", true, false},
        {"ic", "IC / ID number", false, false},
        {"email", "Email", false, false},
        {"telNo", "Telephone", false, false},
        {"address", "Address", false, false},
        {"city", "City", false, false},
        {"postcode", "Postcode", false, false},
        {"state", "State", false, false},
        {"country", "Country", false, false},
        {"dob", "Date of birth", false, false},
        {"gender", "Gender", false, false},
        {"race", "Race", false, false},
        {"religion", "Religion", false, false},
        {"maritalStatus", "Marital status", false, false},
        {"bloodType", "Blood type", false, false},
        {"job", "Occupation", false, false},
        {"enrollNumber", "Enrolment number", false, false},
        {"pointBalance", "Reward point balance", false, true},
        {"discount", "Discount %", false, true},
        {"notes", "Notes", false, false},
    };

    static const std::vector<FieldDef> customer = {
        {"code", "Customer code", false, false},
        {"name", "Name", true, false},
        {"ic", "IC / Registration No (SSM)", false, false},
        {"email", "Email", false, false},
        {"telNo", "Telephone", false, false},
        {"faxNo", "Fax", false, false},
        {"contact", "Contact person", false, false},
        {"address", "Address", false, false},
        {"city", "City", false, false},
        {"postcode", "Postcode", false, false},
        {"state", "State", false, false},
        {"country", "Country", false, false},
        {"tin", "TIN", false, false},
        {"notes", "Notes", false, false},
        {"discount", "Discount %", false, true},
        {"creditTerm", "Credit term (days)", false, true},
        {"creditLimit", "Credit limit", false, true},
        {"isTaxable", "Taxable (yes/no)", false, false},
    };

    static const std::vector<FieldDef> supplier = {
        {"code", "Supplier code", false, false},
        {"name", "Name", true, false},
        {"ic", "Registration No (SSM)", false, false},
        {"contact", "Contact person", false, false},
        {"telNo", "Telephone", false, false},
        {"faxNo", "Fax", false, false},
        {"email", "Email", false, false},
        {"address", "Address", false, false},
        {"city", "City", false, false},
        {"postcode", "Postcode", false, false},
        {"state", "State", false, false},
        {"country", "Country", false, false},
        {"tin", "TIN", false, false},
        {"notes", "Notes", false, false},
        {"creditLimit", "Credit limit", false, true},
        {"creditTerm", "Credit term (days)", false, true},
        {"isTaxable", "Taxable (yes/no)", false, false},
    };

    switch (kind) {
        case Kind::Stock: return stock;
        case Kind::Member: return member;
        case Kind::Customer: return customer;
        case Kind::Supplier: return supplier;
    }
    return stock;
}

inline const FieldDef* FindField(Kind kind, const std::string& key) {
    for (const auto& field : Fields(kind))
        if (key == field.key) return &field;
    return nullptr;
}

} // namespace MasterData
