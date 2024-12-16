from fastapi import FastAPI, Form, UploadFile, File, HTTPException, Request, Depends
from fastapi.responses import HTMLResponse, JSONResponse, RedirectResponse
from fastapi.templating import Jinja2Templates
from fastapi.middleware.cors import CORSMiddleware
from supabase import create_client, Client
from typing import Optional
import os
from dotenv import load_dotenv
import logging
from datetime import datetime
from uuid import UUID 

load_dotenv()
app = FastAPI()
templates = Jinja2Templates(directory="templates")

# Initialize Supabase client
supabase: Client = create_client(
    os.getenv("SUPABASE_URL"),
    os.getenv("SUPABASE_KEY")
)

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Auth dependency
async def get_current_user(request: Request):
    token = request.cookies.get("sb-access-token")
    if not token:
        raise HTTPException(status_code=401, detail="Not authenticated")
    try:
        user = supabase.auth.get_user(token)
        return user.user  # Return user data directly
    except Exception:
        raise HTTPException(status_code=401, detail="Invalid or expired token")

from fastapi.exceptions import HTTPException

@app.exception_handler(HTTPException)
async def auth_exception_handler(request: Request, exc: HTTPException):
    if exc.status_code == 401:
        return RedirectResponse(url="/login", status_code=303)
    return JSONResponse(
        status_code=exc.status_code,
        content={"detail": exc.detail}
    )

# app.py - Update the root endpoint
@app.get("/", response_class=HTMLResponse)
async def root(request: Request):
    token = request.cookies.get("sb-access-token")
    if not token:
        return RedirectResponse(url="/login", status_code=303)
    try:
        user = supabase.auth.get_user(token)
        return RedirectResponse(url="/form", status_code=303)  # Changed from "/dashboard" to "/form"
    except Exception:
        return RedirectResponse(url="/login", status_code=303)

@app.get("/form", response_class=HTMLResponse)
async def form_page(request: Request, user=Depends(get_current_user)):
    return templates.TemplateResponse("form.html", {"request": request})

@app.post("/submit/")
async def submit_form(
    request: Request,
    name: str = Form(...),
    type: str = Form(...),
    financial_aid: str = Form(...),
    safety_reach: str = Form(...),
    ed_rd_deadline: str = Form(...),
    css_deadline: str = Form(...),
    sat_required: str = Form(...),  # Add new field
    sat_range: str = Form(...),     # Add new field
    user=Depends(get_current_user)
):
    try:
        result = supabase.table("entries").insert({
            "user_id": user.id,
            "name": name,
            "type": type,
            "financial_aid": financial_aid,
            "safety_reach": safety_reach,
            "ed_rd_deadline": ed_rd_deadline,
            "css_deadline": css_deadline,
            "sat_required": sat_required,
            "sat_range": sat_range,
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S")  # Format time
        }).execute()
        
        if not result.data:
            raise ValueError("Failed to insert data")
            
        return RedirectResponse(url="/dashboard", status_code=303)
    except Exception as e:
        logging.error(f"Error in submit_form: {e}")
        return templates.TemplateResponse(
            "form.html",
            {
                "request": request, 
                "error": "An error occurred while submitting the form."
            }
        )

@app.get("/auth/status")
async def auth_status(request: Request):
    token = request.cookies.get("sb-access-token")
    try:
        if token:
            user = supabase.auth.get_user(token)
            return {"authenticated": True}
    except Exception:
        pass
    return {"authenticated": False}

@app.get("/login", response_class=HTMLResponse)
async def login_page(request: Request):
    return templates.TemplateResponse("login.html", {"request": request})

@app.get("/register", response_class=HTMLResponse)
async def register_page(request: Request):
    return templates.TemplateResponse("register.html", {"request": request})

@app.post("/auth/register")
async def register(
    request: Request,
    email: str = Form(...),
    password: str = Form(...)
):
    try:
        user = supabase.auth.sign_up({
            "email": email,
            "password": password
        })
        return templates.TemplateResponse(
            "register.html", 
            {
                "request": request, 
                "success": "Registration successful! Please check your email for confirmation."
            }
        )
    except Exception as e:
        return templates.TemplateResponse(
            "register.html", 
            {"request": request, "error": "Registration failed. Please try again."}
        )

@app.post("/auth/login")
async def login(
    request: Request,
    email: str = Form(...),
    password: str = Form(...)
):
    try:
        auth = supabase.auth.sign_in_with_password({
            "email": email,
            "password": password
        })
        response = RedirectResponse(url="/form", status_code=303)  # Changed from "/" to "/form"
        response.set_cookie(
            key="sb-access-token",
            value=auth.session.access_token,
            httponly=True,
            secure=True,
            samesite="lax",
            max_age=3600 * 24 * 30  # 30 days cookie expiry
        )
        return response
    except Exception as e:
        return templates.TemplateResponse(
            "login.html", 
            {"request": request, "error": "Invalid email or password."}
        )

# Logout route
@app.get("/logout")
async def logout(request: Request):
    response = RedirectResponse(url="/login", status_code=303)
    response.delete_cookie("sb-access-token")
    return response

@app.get("/dashboard", response_class=HTMLResponse)
async def dashboard(request: Request, user=Depends(get_current_user)):
    try:
        response = supabase.table("entries").select("*").eq("user_id", user.id).execute()
        entries = response.data
        return templates.TemplateResponse("dashboard.html", {
            "request": request, 
            "entries": entries,
            "user": user
        })
    except Exception as e:
        logging.error(f"Error in dashboard: {e}")
        return templates.TemplateResponse(
            "dashboard.html",
            {"request": request, "error": "Failed to load entries."}
        )

@app.post("/entry/{entry_id}/delete")
async def delete_entry(entry_id: str, user=Depends(get_current_user)):  # Change type to str
    try:
        result = supabase.table("entries").delete().eq("id", entry_id).eq("user_id", user.id).execute()
        if not result.data:
            raise ValueError("Entry not found or unauthorized")
        return RedirectResponse(url="/dashboard", status_code=303)
    except Exception as e:
        logging.error(f"Error in delete_entry: {e}")
        return RedirectResponse(
            url=f"/dashboard?error={str(e)}", 
            status_code=303
        )

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="127.0.0.1", port=8000)