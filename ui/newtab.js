(() => {
  const clock=document.getElementById("clock"),date=document.getElementById("date");
  const update=()=>{
    const now=new Date();
    clock.textContent=new Intl.DateTimeFormat(undefined,{hour:"2-digit",minute:"2-digit",hour12:false}).format(now);
    date.textContent=new Intl.DateTimeFormat(undefined,{weekday:"long",day:"numeric",month:"long",year:"numeric"}).format(now);
  };
  update();window.setInterval(update,15000);
  const input=document.querySelector(".search input");
  window.addEventListener("keydown",event=>{
    if(event.key==="/"&&document.activeElement!==input){event.preventDefault();input.focus();}
  });
})();
